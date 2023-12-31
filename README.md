# simple-database
A simple database library.

## About
simple-database (abbreviated SDB) is a simple database library that strives to be as transparent and well... simple, as possible.

The database format allows for blazingly quick iteration and searching through database rows.

## Basic Usage
Storing data:
```cpp
// Student ID, Student Name, Student Email
sdb::Database db({ sdb::DatabaseColumnType::UI32, sdb::DatabaseColumnType::String, sdb::DatabaseColumnType::String });
    
db.create_row().set_column(0, 1234).set_column(1, std::string("John Doe")).set_column(2, std::string("johndoe@school.nl"));
db.create_row().set_column(0, 5678).set_column(1, std::string("Simon Adams")).set_column(2, std::string("simonadams@school.nl"));
    
db.write_to_file("my_database.sdb");
```

Loading & Iterating database:
```cpp
sdb::Database db("my_database.sdb");

for (sdb::Database::RowView row : db)
{
    std::cout << row.get_column<uint32_t>(0);
    std::cout << row.get_column<std::string>(1);
}
```

Querying database:
```cpp
sdb::Database db("my_database.sdb");

// Get all rows where the first column is equal to 1234.
sdb::Database::Query query = db.query().where(0, 1234);

for (RowView row : query.selection)
{
    row.do_someting();
}
```

## File format
SDB uses a simple binary format to store databases as files. Each file consists of only one database table. Each database file is split into three sections; metadata, string table and row table. More information on individual sections will be discussed later.

Strings are not stored along with the other columns, rather, enough space gets allocated in the string table section of the database, and the string gets loaded into said allocation. Then the address (starting from the beginning of the string table) gets stored along with the other columns as an 8-byte integer. This allows the rows to be a fixed length in bytes, and thus lets us use efficient iteration through the rows with an offset based algorithm.

Take for example the following database, it consists of three columns: Student ID, Student Name and Student Email:

| Student ID | Student Name | Student Email        |
|------------|--------------|----------------------|
| 1234       | John Doe     | johndoe@school.nl    |
| 5678       | Simon Adams  | simonadams@school.nl |

We see that the two rows aren't the same size. In both rows, the student ID is 4 bytes. The first row has a name of 8 bytes, and an email of 17 bytes. The second row has a name of 11 bytes and an email of 20 bytes. The first row is 29 bytes and the second row is 35 bytes. In this case, iterating through the rows would take a lot of effort, as we'd need to check how long each row is before we're able to iterate to the next one. This complexity adds up in large databases. However, there is a simple solution; delegating strings to a seperate section.

String table section:
```
John Doe\0johndoe@school.nl\0Simon Adams\0simonadams@school.nl\0
| 0       | 9                | 27         | 39
```

Row table section:
| Student ID | Student Name | Student Email |
|------------|--------------|---------------|
| 1234       | 0            | 9             |
| 5678       | 27           | 39            |

Now each row is exactly 20 bytes long (4-byte student ID, two 8-byte string addresses). And iterating is as simple as multiplying the row size by the row index.


### Metadata
The metadata section is the first section in the database file, it consists of tree parts; column count, column types and string table size.

The column count consists of two bytes, allowing a range of up to 2^16 - 1 (65535) columns. 

The column types consist of N single byte integers, N being the column count, allowing a range of up to 2^8 - 1 (255) column types. The currently implemented column types are as follows:

| Type Number | Type Name | Long Name                                     | Byte Size |
|------------:|-----------|-----------------------------------------------|----------:|
|          00 | Bool      | Boolean                                       |         1 |
|          01 | UI8       | Unsigned 8-bit Integer                        |         1 |
|          02 | I8        | Signed 8-bit Integer                          |         1 |
|          03 | UI16      | Unsigned 16-bit Integer                       |         2 |
|          04 | I16       | Signed 16-bit Integer                         |         2 |
|          05 | UI32      | Unsigned 32-bit Integer                       |         4 |
|          06 | I32       | Signed 32-bit Integer                         |         4 |
|          07 | UI64      | Unsigned 64-bit Integer                       |         8 |
|          08 | I64       | Signed 64-bit Integer                         |         8 |
|          09 | F32       | 32-bit Single Precision Floating Point Number |         4 |
|          0A | F64       | 64-bit Double Precision Floating Point Number |         8 |
|          0B | String    | An array of unlimited count of character.     |         8 |

The string table size is an 8 byte integer representing the total size of the string table section in bytes. This value is used to calculate the beginning of the row table section.

### String table
The string table section starts directly after the metadata section and grows downward. The string table is not a table, it's more like a tightly packed array of character arrays. The strings are seperated from eachother using null-terminator bytes.

### Row table
The row table section starts directly after the string table section and grows downward. The row table section is always a multiple of the row size in bytes in size.
