cmake_minimum_required(VERSION 3.21)

project(simple-database VERSION 0.0.1 DESCRIPTION "Simple Database")

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build library and test executable in same directory.
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set(SDB_SOURCES
	sdb.cpp
)
list(TRANSFORM SDB_SOURCES PREPEND "src/")

#set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source to this project's executable.
add_library(simple-database SHARED ${SDB_SOURCES})
target_include_directories(simple-database PUBLIC include)

add_subdirectory(test)
