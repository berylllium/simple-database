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

    // Test 2: String columns & iterators.
    sdb::Database db3({ sdb::DatabaseColumnType::UI32, sdb::DatabaseColumnType::String });

    db3.create_row().set_column(0, 2).set_column(1, std::string("Hello there :)"));
    db3.create_row().set_column(0, 3).set_column(1, std::string("This is another row."));
    db3.create_row().set_column(0, 4).set_column(1, std::string("This is a longer string, and also another row."));

    for (sdb::Database::RowView row : db3)
    {
        std::cout << row.get_column<std::string>(1);
    }

    db3.write_to_file("test3.sdb");

    // Test 3: Query database.

    sdb::Database db4({ sdb::DatabaseColumnType::UI32, sdb::DatabaseColumnType::String});

    db4.create_row().set_column(0, 1234).set_column(1, std::string("This string belongs to 1234."));
    db4.create_row().set_column(0, 5678).set_column(1, std::string("This one to 5678."));
    db4.create_row().set_column(0, 1234).set_column(1, std::string("Another 1234."));

    sdb::Database::Query q = db4.query().where(0, 1234);

    for (auto row : q.selection)
    {
        std::cout << row.get_column<std::string>(1);
    }

	return 0;
}
