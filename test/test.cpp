#include <iostream>
#include <sdb.hpp>

int main()
{
    // Test 1: Simple metadata.
    sdb::Database db("test.sdb");

    std::cout << db.column_count << '\n';

    for (size_t i = 0; i < db.column_count; i++)
    {
        std::cout << static_cast<int>(db.column_types[i]) << ' ';
    }

    std::cout << '\n' << db.string_table.size() << '\n';

    // Test 2: Integer columns.
    sdb::Database db2({ sdb::DatabaseColumnType::UI32, sdb::DatabaseColumnType::UI32 });

    for (size_t i = 0; i < 100; i++)
    {
        db2.create_row().set_column(0, i).set_column(1, i + 1);
    }

    db2.write_to_file("test2.sdb");

    // Test 2: String columns.
    sdb::Database db3({ sdb::DatabaseColumnType::UI32, sdb::DatabaseColumnType::String });

    db3.create_row().set_column(0, 2).set_column(1, std::string("Hello there :)"));
    db3.create_row().set_column(0, 3).set_column(1, std::string("This is another row."));
    db3.create_row().set_column(0, 4).set_column(1, std::string("This is a longer string, and also another row."));

    db3.write_to_file("test3.sdb");

	return 0;
}
