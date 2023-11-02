#include "sdb.hpp"

#include <algorithm>

namespace sdb
{

void Database::Query::remove_selection()
{
    // Remove selection from highest, to lowest offset.
    // Copy offsets to temporary vector.
    std::vector<uint64_t> offsets;
    offsets.reserve(selection.size());
    
    for (RowView& row : selection)
    {
        offsets.push_back(row.row_offset);
    }
    
    // Sort offsets from greatest to lowest.
    std::sort(offsets.begin(), offsets.end(), std::greater<uint64_t>());

    // Remove rows, starting with higher offsets.
    for (uint64_t offset : offsets)
    {
        db->row_table.erase(
            db->row_table.begin() + offset,
            db->row_table.begin() + offset + db->row_size);
    }

    // Shrink row table.
    db->row_table.shrink_to_fit();
}

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
    size_t new_row_offset = row_table.size();

    // Allocate enough memory for new row.
    row_table.resize(row_table.size() + row_size);

    RowView row_view(new_row_offset, this);

    // Set default values.
    for (size_t i = 0; i < column_count; i++)
    {
        switch (column_types[i])
        {
        case DatabaseColumnType::String:
        {
            row_view.set_column<uint64_t>(i, UINT64_MAX);
        } break;
        default:
            break;
        }
    }

    return std::move(row_view);
}

Database::Query Database::query()
{
    return Query(this);
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
    return Iterator(0, this);
}

Database::Iterator Database::end()
{
    return Iterator(row_table.size(), this);
}

size_t Database::add_string(std::string s)
{
    // Calculate neccessary chunks to store string.
    size_t string_size = s.size() + 1; // Null-terminator.
    size_t needed_chunks = string_size / string_chunk_size;
    if (string_size % string_chunk_size > 0) needed_chunks++;

    // Return if no chunks are required.
    if (needed_chunks == 0) return 0;

    // Reserve chunks.
    size_t satisfied_chunks = 0;
    std::unique_ptr<uint64_t[]> reserved_chunk_offsets = std::make_unique<uint64_t[]>(needed_chunks);

    // Find empty existing chunks.
    size_t actual_string_chunk_size = (string_chunk_size + sizeof(uint64_t));

    for (size_t current_chunk = 0; current_chunk < (string_table.size() / actual_string_chunk_size); current_chunk++)
    {
        size_t current_chunk_offset = current_chunk * actual_string_chunk_size;

        if (*reinterpret_cast<uint64_t*>(string_table.data() + current_chunk_offset + string_chunk_size) == UINT64_MAX)
        {
            // Empty chunk found.
            reserved_chunk_offsets[satisfied_chunks] = current_chunk_offset;

            satisfied_chunks++;
        }

        if (satisfied_chunks == needed_chunks) break;
    }

    // Allocate enough memory for the unsatisfied chunks.
    size_t unsatisfied_chunks = needed_chunks - satisfied_chunks;

    if (unsatisfied_chunks > 0)
    {
        size_t first_allocated_chunk_offset = string_table.size();

        string_table.resize(string_table.size() + unsatisfied_chunks * actual_string_chunk_size);

        for (size_t i = 0; i < unsatisfied_chunks; i++)
        {
            reserved_chunk_offsets[satisfied_chunks + i] = first_allocated_chunk_offset + i * actual_string_chunk_size;
        }
    }

    // Populate chunks.
    for (size_t i = 0; i < needed_chunks; i++)
    {
        // Setup pointers.
        if (i < (needed_chunks - 1))
        {
            // Point the current chunk to the next chunk if this isn't the last chunk.
            *reinterpret_cast<uint64_t*>(string_table.data() + reserved_chunk_offsets[i] + string_chunk_size) =
                reserved_chunk_offsets[i + 1];
        }
        else
        {
            // Set pointer to point to the first chunk if this is the last chunk.
            *reinterpret_cast<uint64_t*>(string_table.data() + reserved_chunk_offsets[i] + string_chunk_size) =
                reserved_chunk_offsets[0];
        }

        // Populate chunk.
        // Calculate how many strings to copy. The last chunk can contain less than string_chunk_size characters.
        size_t copy_size = string_chunk_size;
        if (i == (needed_chunks - 1))
        {
            // Last chunk.
            if (string_size % string_chunk_size > 0)
            {
                copy_size = string_size % string_chunk_size;
            }
        }

        // Copy over characters from the string.
        std::memcpy(
            string_table.data() + reserved_chunk_offsets[i],
            s.data() + i * string_chunk_size,
            copy_size);
        
    }

    return reserved_chunk_offsets[0];
}

std::string Database::get_string(size_t offset) const
{
    std::string s;

    size_t first_offset = offset;
    size_t next_offset = UINT64_MAX;

    while (next_offset != first_offset)
    {
        next_offset = *reinterpret_cast<const uint64_t*>(string_table.data() + offset + string_chunk_size);

        if (next_offset == first_offset)
        {
            // Last chunk.
            s += std::string(reinterpret_cast<const char*>(string_table.data() + offset));
        }
        else
        {
            // Full chunk is string.
            s += std::string(reinterpret_cast<const char*>(string_table.data() + offset), string_chunk_size);
        }

        offset = next_offset;
    }

    return s;
}

void Database::remove_string(size_t offset)
{
    size_t first_offset = offset;
    size_t next_offset = UINT64_MAX;

    while (next_offset != first_offset)
    {
        next_offset = *reinterpret_cast<uint64_t*>(string_table.data() + offset + string_chunk_size);

        *reinterpret_cast<uint64_t*>(string_table.data() + offset + string_chunk_size) = UINT64_MAX;

        offset = next_offset;
    }
}

size_t Database::get_column_type_size(DatabaseColumnType type)
{
    switch (type)
    {
    case DatabaseColumnType::Bool:
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
