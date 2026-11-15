# einstein_storage：存档持久化（SQLite）。
# PUBLIC 依赖 core：save_manager.h 在接口上暴露 core::GameState。
# PRIVATE 依赖 sqlite3：sqlite_wrapper.h 用前向声明隐藏了 sqlite3 头文件，
# 所以 sqlite3 只需链接给本库自身，不会传递给上层。
add_library(einstein_storage STATIC save_manager.cpp sqlite_wrapper.cpp)
target_include_directories(einstein_storage PUBLIC ..)
target_link_libraries(einstein_storage PUBLIC einstein_core PRIVATE sqlite3 einstein_warnings)