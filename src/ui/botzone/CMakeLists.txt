add_executable(einstein_botzone main_botzone.cpp)

target_include_directories(einstein_botzone PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(einstein_botzone PRIVATE
    einstein_core
    einstein_ai
    einstein_warnings
)