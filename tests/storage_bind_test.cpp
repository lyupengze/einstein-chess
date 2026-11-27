/// @file tests/storage_bind_test.cpp
/// @brief SQLite 参数绑定回归：绑定失败必须抛异常，不能继续执行语句。
///
/// 越界下标稳定返回 SQLITE_RANGE，是不依赖内存不足的失败入口；整数、浮点、
/// 文本与 NULL 四种重载都要走同一套检查。绑定失败后语句没有执行，表里不能
/// 留下带空值的行。回归背景：见 `#storage-bind-ignored`。

#include <cstdio>
#include <stdexcept>
#include <string>

#include "storage/sqlite_wrapper.h"
#include "test_support.h"

using test_support::expect;

namespace {

constexpr const char* kDbPath = "test_storage_bind.db";

// 四列覆盖整数、浮点、文本与 NULL 四种绑定重载。
constexpr const char* kInsertSql = "INSERT INTO sample (i, r, t, n) VALUES (?, ?, ?, ?);";

void create_table(storage::Database& db) {
    storage::Statement stmt(db, "CREATE TABLE sample (i INTEGER, r REAL, t TEXT, n TEXT);");
    stmt.step();
}

int row_count(storage::Database& db) {
    storage::Statement stmt(db, "SELECT COUNT(*) FROM sample;");
    if (!stmt.step()) {
        return -1;
    }
    return stmt.column_int(0);
}

// 每条失败用例都用新语句：绑定失败后不复用同一份绑定状态。
template <typename BindCall>
bool bind_fails(storage::Database& db, BindCall bind_call) {
    storage::Statement stmt(db, kInsertSql);
    try {
        bind_call(stmt);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    std::remove(kDbPath);
    {
        storage::Database db(kDbPath);
        create_table(db);

        {
            storage::Statement stmt(db, kInsertSql);
            stmt.bind(1, 42);
            stmt.bind(2, 1.25);
            stmt.bind(3, std::string("abc"));
            stmt.bind_null(4);
            expect(!stmt.step(), "合法绑定可以执行到完成");
        }
        {
            storage::Statement stmt(db, "SELECT i, r, t, n IS NULL FROM sample;");
            expect(stmt.step(), "能读回刚写入的行");
            expect(stmt.column_int(0) == 42, "整数绑定原样写回");
            expect(stmt.column_double(1) == 1.25, "浮点绑定原样写回");
            expect(stmt.column_text(2) == "abc", "文本绑定原样写回");
            expect(stmt.column_int(3) == 1, "bind_null 写入 NULL");
        }

        const int rows_before = row_count(db);
        expect(rows_before == 1, "写入后表里只有一行");

        // 语句只有 4 个占位符，合法下标为 1..4；0 与 5 都应返回 SQLITE_RANGE。
        expect(bind_fails(db, [](storage::Statement& s) { s.bind(0, 1); }),
               "整数绑定的下标 0 被拒绝");
        expect(bind_fails(db, [](storage::Statement& s) { s.bind(5, 1); }),
               "整数绑定的越界下标被拒绝");
        expect(bind_fails(db, [](storage::Statement& s) { s.bind(0, 1.25); }),
               "浮点绑定的下标 0 被拒绝");
        expect(bind_fails(db, [](storage::Statement& s) { s.bind(5, 1.25); }),
               "浮点绑定的越界下标被拒绝");
        expect(bind_fails(db, [](storage::Statement& s) { s.bind(0, std::string("x")); }),
               "文本绑定的下标 0 被拒绝");
        expect(bind_fails(db, [](storage::Statement& s) { s.bind(5, std::string("x")); }),
               "文本绑定的越界下标被拒绝");
        expect(bind_fails(db, [](storage::Statement& s) { s.bind_null(0); }),
               "NULL 绑定的下标 0 被拒绝");
        expect(bind_fails(db, [](storage::Statement& s) { s.bind_null(5); }),
               "NULL 绑定的越界下标被拒绝");

        std::string message;
        try {
            storage::Statement stmt(db, kInsertSql);
            stmt.bind(5, 1);
        } catch (const std::runtime_error& error) {
            message = error.what();
        }
        expect(message.find("绑定参数失败") != std::string::npos, "失败信息说明是参数绑定出错");

        expect(row_count(db) == rows_before, "绑定失败没有写入新行");

        // reset 后可重新绑定并再次执行；绑定值必须被清空，否则第二次会带着上一次的文本。
        {
            storage::Statement stmt(db, kInsertSql);
            stmt.bind(1, 7);
            stmt.bind(2, 2.5);
            stmt.bind(3, std::string("first"));
            stmt.bind_null(4);
            expect(!stmt.step(), "第一次执行写入一行");

            stmt.reset();
            stmt.bind(1, 8);
            stmt.bind(2, 3.5);
            expect(!stmt.step(), "reset 后重新绑定可以再次执行");
        }
        {
            storage::Statement stmt(db, "SELECT i, r, t, n IS NULL FROM sample WHERE i = 8;");
            expect(stmt.step(), "能读回 reset 后写入的行");
            expect(stmt.column_int(0) == 8 && stmt.column_double(1) == 3.5,
                   "重新绑定的列按新值写入");
            expect(stmt.column_text(2).empty() && stmt.column_int(3) == 1,
                   "reset 清空了上一次的文本与 NULL 绑定");
        }
    }
    std::remove(kDbPath);

    return test_support::finish();
}
