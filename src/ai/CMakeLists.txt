# einstein_ai：布阵与走子决策库，只依赖 core。
# PUBLIC 依赖 core：ai.h 的头文件中直接出现 core::GameState / core::Pos，
# 使用方必须同时能看到 core 的头文件。
add_library(einstein_ai STATIC ai.cpp)
target_include_directories(einstein_ai PUBLIC ..)
target_link_libraries(einstein_ai PUBLIC einstein_core)
target_link_libraries(einstein_ai PRIVATE einstein_warnings)