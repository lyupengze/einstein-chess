add_library(einstein_core STATIC board.cpp rules.cpp game.cpp)
target_include_directories(einstein_core PUBLIC ..)
target_link_libraries(einstein_core PRIVATE sqlite3)
target_link_libraries(einstein_core PRIVATE einstein_warnings)