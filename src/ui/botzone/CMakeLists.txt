# Botzone 协议库：只依赖 core + ai，不碰 stdin/stdout，供可执行目标与单元测试共用。
# third_party 作为 PUBLIC include，是因为公共头文件直接暴露 nlohmann::json。
add_library(einstein_botzone_protocol STATIC botzone_protocol.cpp)

target_include_directories(einstein_botzone_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(einstein_botzone_protocol
    PUBLIC einstein_core einstein_ai
    PRIVATE einstein_warnings
)

# einstein_botzone：平台适配可执行目标（无状态，单次请求-响应）。
# 只需协议库；不链接 storage，因为平台侧不使用本地存档。
add_executable(einstein_botzone main_botzone.cpp)

target_link_libraries(einstein_botzone PRIVATE
    einstein_botzone_protocol
    einstein_warnings
)
