#pragma once

#include <cstdint>
#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <initializer_list>

namespace sdb
{

class DatabaseNotFoundException : public std::exception
{
public:
    virtual const char* what() const noexcept override 
    {
        return "Tried opening non-existent database file.";
    }
};

enum class DatabaseColumnType
{
    UI8, I8,
    UI16, I16,
    UI32, I32,
    UI64, I64,

    F32, F64,

    String
};

struct Database
{
    struct RowView
    {
        template<typename T>
        RowView& set_column(size_t i, const T& v)
        {
            if constexpr (std::is_same_v<T, std::string>)
            {
                // Increase string table memory.
                size_t string_address = database->add_string(v);

                *reinterpret_cast<uint64_t*>(columns[i]) = string_address;
            }
            else
            {
                *reinterpret_cast<T*>(columns[i]) = v;
            }

            return *this;
        }

        template<typename T>
        T get_column(size_t i) const
        {
            // Workaround until GCC fixes the explicit template specialization bug.
            // https://open-std.org/JTC1/SC22/WG21/docs/cwg_defects.html#727
            if constexpr (std::is_same_v<T, std::string>)
            {
                uint64_t string_address = *reinterpret_cast<uint64_t*>(columns[i]);

                return reinterpret_cast<char*>(database->string_table[string_address]);
            }
            else
            {
                return *reinterpret_cast<T*>(columns[i]);
            }
        }

//        // Postpone this until GCC fixes this stupid bug...
//        template<>
//        std::string get_column<std::string>(size_t i) const
//        {
//            uint64_t string_address = *reinterpret_cast<uint64_t*>(columns[i]);
//
//            return reinterpret_cast<char*>(database->string_table[string_address]);
//        }

    private:
        std::unique_ptr<uint8_t*[]> columns;

        Database* database;

        RowView() {}

        friend Database;
    };

    uint16_t column_count; 
    std::unique_ptr<DatabaseColumnType[]> column_types;

    std::vector<uint8_t> string_table;

    std::vector<uint8_t> row_table;

    Database(std::initializer_list<DatabaseColumnType> column_types);
    Database(std::string file_name);

    RowView create_row();

    void write_to_file(std::string file_name) const;

    size_t get_metadata_size() const;
    size_t get_string_table_offset() const;
    size_t get_row_table_offset() const;

private:
    size_t row_size = 0;

    void populate_row_view(RowView& row_view, size_t row_table_address);

    size_t add_string(std::string s);

    static size_t get_column_type_size(DatabaseColumnType type);
};
    
}
