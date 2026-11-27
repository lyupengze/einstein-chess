/// @file src/storage/sqlite_wrapper.h
/// @brief SQLite C API 的极简 RAII 封装，只覆盖本项目用到的子集。
///
/// 把裸句柄的所有权收进类中（析构时自动 close / finalize），业务代码
/// （`save_manager`）不必关心 C API 的清理细节；失败一律转成
/// `std::runtime_error`，消息里带上 sqlite3 的错误描述。

#pragma once

#include <string>

// 只前向声明，不包含 <sqlite3.h>：否则会把三方头文件泄漏给所有依赖 storage
// 的目标（编译命令行都要多出三方 include 路径）；需要完整定义的只有 .cpp。
struct sqlite3;
struct sqlite3_stmt;

namespace storage {

/// @brief 数据库连接，构造即打开（文件不存在则创建），析构自动关闭。
/// @note 拷贝被禁用，只允许移动：连接是独占资源，不应出现两份所有权。
/// @note 析构走 `sqlite3_close_v2`：若还有语句未 finalize，连接延后到语句释放时关闭，
///       不会因 `SQLITE_BUSY` 留在打开状态。
class Database {
public:
    /// @param db_path 数据库文件路径（SQLite 语义：不存在则新建）
    /// @throws std::runtime_error 打开失败
    explicit Database(const std::string& db_path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /// 移动后源对象的句柄置空，析构时不会重复关闭。
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    /// 返回裸句柄，仅借用，不转移所有权。
    sqlite3* get() const;

private:
    sqlite3* db_ = nullptr;
};

/// @brief 预编译语句，持有 sqlite3_stmt 的所有权，析构自动 finalize。
class Statement {
public:
    /// 用连接 db 预编译 sql。
    /// @throws std::runtime_error 预编译失败（如 SQL 语法错误）
    Statement(Database& db, const std::string& sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    /// 绑定参数。index 是 1-based（SQLite 约定：占位符 ? 从 1 开始编号）。
    void bind(int index, int value);
    void bind(int index, double value);
    /// 字符串按值拷贝进语句（SQLITE_TRANSIENT），调用方无需保证其生命周期。
    void bind(int index, const std::string& value);
    void bind_null(int index);

    /// 执行一步。
    /// @return true 取到一行（SQLITE_ROW）；false 已执行完毕（SQLITE_DONE）
    /// @throws std::runtime_error 执行出错
    bool step();

    /// 复位语句并清空全部绑定，便于重新绑定后再执行。
    /// @note 不抛异常：`sqlite3_reset` 的返回值是上一次 `step()` 的错误码，该错误已由
    ///       `step()` 上报；复位本身不会失败。
    void reset();

    /// 读取结果列。col 是 0-based（与 bind 的 1-based 规则相反）。
    /// column_text 遇到 SQL NULL 时返回空串。
    int column_int(int col) const;
    double column_double(int col) const;
    std::string column_text(int col) const;

    /// 返回裸句柄，仅借用，不转移所有权。
    sqlite3_stmt* get() const;

private:
    sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace storage
