add_library(einstein_ai STATIC ai.cpp)
target_include_directories(einstein_ai PUBLIC ..)
target_link_libraries(einstein_ai PUBLIC einstein_core)
target_link_libraries(einstein_ai PRIVATE einstein_warnings)