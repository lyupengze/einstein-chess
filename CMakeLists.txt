cmake_minimum_required(VERSION 4.0.0)

project(einstein_chess LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(third_party/sqlite)

add_library(einstein_warnings INTERFACE)
target_compile_options(einstein_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror $<$<CONFIG:Debug>:-Og -g> $<$<NOT:$<CONFIG:Debug>>:-O2>)

add_subdirectory(src/core)
add_subdirectory(src/storage)
add_subdirectory(src/ai)

add_subdirectory(src/ui/cli)



# add_executable(einstein_botzone src/botzone/main_botzone.cpp)
# target_link_libraries(einstein_botzone PRIVATE einstein_core)

# add_executable(einstein_gui src/gui/main_gui.cpp)
# target_link_libraries(einstein_gui PRIVATE einstein_core Qt6::Widgets)