cmake_minimum_required(VERSION 4.0.0)

project(einstein_chess LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(third_party/sqlite)

add_executable(main src/main.cpp src/core/board.cpp src/core/rules.cpp)

target_compile_options(main PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror -O2)
target_link_libraries(main PRIVATE sqlite3)