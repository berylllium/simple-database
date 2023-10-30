#include "sdb.hpp"

namespace sdb
{

Database::Database(std::initializer_list<DatabaseColumnType> column_types)
{
    if (column_types.size() > 0)
    {
        column_count = column_types.size();
        this->column_types = std::make_unique<DatabaseColumnType[]>(column_count);

        uint16_t i = 0;
        for (auto column_type : column_types)
        {
            this->column_types[i] = column_type;

            row_size += get_column_type_size(column_type);

            i++;
        }
    }
}

Database::Database(std::string file_name)
{
    // Open database file.
    std::fstream db_file(file_name, std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
    
    // Throw if fail.
    if (db_file.fail())
    {
        throw DatabaseNotFoundException();
    }
    
    // Get database file size.
    std::streamsize data_size = db_file.tellg();
    db_file.seekg(0, std::ios::beg);
    
    // Read database into memory.
    std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(data_size);
    
    db_file.read(reinterpret_cast<char*>(data.get()), data_size);
    
    // Get metadata.
    // Get column count.
    column_count = *reinterpret_cast<uint16_t*>(data.get());
    
    // Get column types.
    column_types = std::make_unique<DatabaseColumnType[]>(column_count);
    
    for (uint16_t i = 0; i < column_count; i++)
    {
        DatabaseColumnType type = static_cast<DatabaseColumnType>(*(data.get() + sizeof(uint16_t) + i));
        column_types[i] = type;
        row_size += get_column_type_size(type);
    }
    
    // Get string table size.
    uint64_t string_table_size = *reinterpret_cast<uint64_t*>(data.get() + sizeof(uint16_t) + column_count);
    
    if (string_table_size > 0)
    {
        // Load string table.
        string_table.resize(string_table_size);

        std::memcpy(string_table.data(), data.get() + get_metadata_size(), string_table_size);
    }

    // Get row table size.
    size_t metadata_size = get_metadata_size();
    uint64_t row_table_size = data_size - metadata_size - string_table_size;

    if (row_table_size > 0)
    {
        // Load row table.
        row_table.resize(row_table_size);

        std::memcpy(row_table.data(), data.get() + get_metadata_size() + string_table_size, row_table_size);
    }
}

Database::RowView Database::create_row()
{
    size_t new_row_address = row_table.size();

    // Allocate enough memory for new row.
    row_table.resize(row_table.size() + row_size);

    RowView row_view(row_table.data() + new_row_address, this);

    return std::move(row_view);
}

void Database::write_to_file(std::string file_name) const
{
    // Open file.
    std::ofstream file(file_name, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);

    // Write metadata.
    // Column count.
    file.write(reinterpret_cast<const char*>(&column_count), sizeof(uint16_t));

    // Column types.
    for (size_t i = 0; i < column_count; i++)
    {
        file.write(reinterpret_cast<const char*>(&column_types[i]), sizeof(uint8_t));
    }

    // String table size.
    uint64_t string_table_size = string_table.size();
    file.write(reinterpret_cast<const char*>(&string_table_size), sizeof(uint64_t));

    // String table.
    file.write(reinterpret_cast<const char*>(string_table.data()), string_table.size());

    // Row table.
    file.write(reinterpret_cast<const char*>(row_table.data()), row_table.size());
}

size_t Database::get_metadata_size() const
{
    return sizeof(uint16_t) + column_count + sizeof(uint64_t);
}

size_t Database::get_string_table_offset() const
{
    return get_metadata_size();
}

size_t Database::get_row_table_offset() const
{
    return get_metadata_size() + string_table.size();
}

Database::Iterator Database::begin()
{
    return Iterator(row_table.data(), this);
}

Database::Iterator Database::end()
{
    return Iterator(row_table.data() + row_table.size(), this);
}

size_t Database::add_string(std::string s)
{
    size_t new_string_address = string_table.size();

    // Allocate new memory for string.
    string_table.resize(string_table.size() + s.size() + 1);

    // Write string to string table.
    std::memcpy(string_table.data() + new_string_address, s.data(), s.size() + 1);

    return new_string_address;
}

size_t Database::get_column_type_size(DatabaseColumnType type)
{
    switch (type)
    {
    case DatabaseColumnType::UI8:
    case DatabaseColumnType::I8:
        return 1;
    case DatabaseColumnType::UI16:
    case DatabaseColumnType::I16:
        return 2;
    case DatabaseColumnType::UI32:
    case DatabaseColumnType::I32:
    case DatabaseColumnType::F32:
        return 4;
    case DatabaseColumnType::UI64:
    case DatabaseColumnType::I64:
    case DatabaseColumnType::F64:
    case DatabaseColumnType::String:
        return 8;
    }

    return 0;
}

}
