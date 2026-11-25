# AI benchmark：只依赖 core/ai，不依赖 Qt，也不注册成 CTest 用例。
# 目标随日常构建一起编译，保证接口变动时能及时发现；是否运行由开发者手动决定。
add_executable(einstein_benchmark ai_benchmark.cpp)
target_link_libraries(einstein_benchmark PRIVATE einstein_ai einstein_core einstein_warnings)
