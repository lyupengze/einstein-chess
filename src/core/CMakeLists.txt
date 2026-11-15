# einstein_core：规则与对局状态库，不依赖 Qt、SQLite 等任何三方组件。
# PUBLIC .. 把 src/ 设为公共 include 根，其他模块统一按 core/xxx.h 引用头文件。
add_library(einstein_core STATIC board.cpp rules.cpp game.cpp)
target_include_directories(einstein_core PUBLIC ..)
target_link_libraries(einstein_core PRIVATE einstein_warnings)