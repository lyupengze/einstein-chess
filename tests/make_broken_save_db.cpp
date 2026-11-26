/// @file tests/make_broken_save_db.cpp
/// @brief 测试辅助程序：创建一个 saves 表结构损坏的存档库。
///
/// SaveManager 构造时执行 CREATE TABLE IF NOT EXISTS saves(...)，不会修复已存在的
/// 残缺表；这里提前建只有一列 x 的 saves 表，随后 list_saves() 查询 name/timestamp
/// 就会失败。cli_broken_db 用例借此验证列存档失败只打印原因并回主菜单。
/// 本程序只由 tests/cli_broken_db_test.cmake 调用，不注册为 CTest 用例。

#include <sqlite3.h>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: make_broken_save_db <database-path>\n";
        return 2;
    }

    const std::string database_path = argv[1];
    sqlite3* database = nullptr;
    if (sqlite3_open(database_path.c_str(), &database) != SQLITE_OK) {
        std::cerr << "无法打开数据库 " << database_path << ": "
                  << (database != nullptr ? sqlite3_errmsg(database) : "unknown error") << "\n";
        if (database != nullptr) {
            sqlite3_close(database);
        }
        return 1;
    }

    char* error_message = nullptr;
    const int result =
        sqlite3_exec(database, "CREATE TABLE saves(x TEXT)", nullptr, nullptr, &error_message);
    if (result != SQLITE_OK) {
        std::cerr << "无法创建损坏的 saves 表: "
                  << (error_message != nullptr ? error_message : "unknown error") << "\n";
        sqlite3_free(error_message);
        sqlite3_close(database);
        return 1;
    }

    sqlite3_close(database);
    return 0;
}
