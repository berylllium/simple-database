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
    Bool,
    
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
                // Check if there already is a string.
                uint64_t current_offset = get_column<uint64_t>(i);

                // Remove old string if exists.
                if (current_offset != UINT64_MAX) database->remove_string(current_offset);

                // Add new string.
                uint64_t new_offset = database->add_string(v);

                *get_column_address<uint64_t>(i) = new_offset;
            }
            else
            {
                *get_column_address<T>(i) = v;
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
                uint64_t string_offset = *get_column_address<uint64_t>(i);

                return database->get_string(string_offset);
            }
            else
            {
                return *get_column_address<T>(i);
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
        size_t row_offset;

        Database* database;

        RowView(size_t row_offset, Database* database) : row_offset(row_offset), database(database) {}

        template<typename T>
        T* get_column_address(size_t i) const
        {
            uint8_t* column_address = database->row_table.data() + row_offset;

            for (size_t n = 0; n < i; n++)
            {
                column_address += get_column_type_size(database->column_types[n]);
            }

            return reinterpret_cast<T*>(column_address);
        }

        friend Database;
    };

    struct Query
    {
        std::vector<RowView> selection;

        template<typename T>
        Query& where(size_t i, T v)
        {
            for (auto row : *db)
            {
                if (row.get_column<T>(i) == v)
                {
                    selection.push_back(row);
                }
            }

            return *this;
        }

        template<typename T>
        Query& with(size_t i, T v)
        {
            for (size_t iter = 0; iter < selection.size(); iter++)
            {
                if (selection[iter].get_column<T>(i) != v)
                {
                    selection.erase(selection.begin() + iter);
                }
            }

            return *this;
        }

        void remove_selection();

    private:
        Query(Database* db) : db(db) {}

        Database* db;

        friend Database;
    };

    struct Iterator 
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        
        Iterator(size_t row_offset, Database* database) : row_offset(row_offset), database(database) {}

        RowView operator*() const { return RowView(row_offset, database); }
        RowView operator->() { return RowView(row_offset, database); }

        Iterator& operator ++ () { row_offset += database->row_size; return *this; }

        Iterator operator ++ (int) { Iterator tmp = *this; ++(*this); return tmp; }

        friend bool operator== (const Iterator& a, const Iterator& b)
        {
            return (a.row_offset == b.row_offset) && (a.database == b.database);
        }

        friend bool operator!= (const Iterator& a, const Iterator& b)
        {
            return (a.row_offset != b.row_offset) || (a.database != b.database);
        }
        
    private:
        size_t row_offset;
        Database* database;
    };

    uint16_t column_count; 
    std::unique_ptr<DatabaseColumnType[]> column_types;

    std::vector<uint8_t> string_table;

    std::vector<uint8_t> row_table;

    Database(std::initializer_list<DatabaseColumnType> column_types);
    Database(std::string file_name);

    RowView create_row();

    Query query();

    void write_to_file(std::string file_name) const;

    size_t get_metadata_size() const;
    size_t get_string_table_offset() const;
    size_t get_row_table_offset() const;

    // Iterators.
    Iterator begin();
    Iterator end();

private:
    size_t row_size = 0;

    const size_t string_chunk_size = 32;

    // String management.
    size_t add_string(std::string s);
    std::string get_string(size_t offset) const;

    void remove_string(size_t offset);

    static size_t get_column_type_size(DatabaseColumnType type);
};
    
}
