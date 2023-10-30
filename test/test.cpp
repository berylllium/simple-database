#include <iostream>
#include <sdb.hpp>

int main()
{
    sdb::Database db("test.sdb");

    std::cout << db.column_count << '\n';

    for (size_t i = 0; i < db.column_count; i++)
    {
        std::cout << static_cast<int>(db.column_types[i]) << ' ';
    }

    std::cout << '\n' << db.string_table.size() << '\n';

    sdb::Database db2({ sdb::DatabaseColumnType::UI32, sdb::DatabaseColumnType::UI32 });

    db2.create_row();

    db2.write_to_file("test2.sdb");

	return 0;
}
