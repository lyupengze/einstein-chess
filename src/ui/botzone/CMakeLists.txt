# einstein_botzone：Botzone 平台适配可执行目标（无状态，单次请求-响应）。
# 只需 core + ai；不链接 storage，因为平台侧不使用本地存档。
# third_party 加入 include 路径是为了直接包含 nlohmann/json 单头文件。
add_executable(einstein_botzone main_botzone.cpp)

target_include_directories(einstein_botzone PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(einstein_botzone PRIVATE
    einstein_core
    einstein_ai
    einstein_warnings
)