# SQLite 3.54.1 amalgamation：直接编译上游 sqlite3.c，不做任何源码修改。
# 只作为 einstein_storage 的私有依赖链接；include 目录是 PUBLIC，因为
# sqlite_wrapper.cpp 需要包含 sqlite3.h，但该头文件不会被上层模块看到。
enable_language(C)

add_library(sqlite3 STATIC sqlite3.c)

target_include_directories(sqlite3 PUBLIC .)
