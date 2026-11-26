/// @file src/storage/sqlite_wrapper.cpp
/// @brief 句柄生命周期与返回码检查；所有失败路径都转成异常。
///
/// C API 的返回码不经调用方逐层传递：打开、预编译、绑定、执行四类失败都在
/// 发生处抛出，业务代码只处理成功路径。

#include "sqlite_wrapper.h"

#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace storage {

namespace {

// 统一检查 sqlite3_bind_* 的返回码。绑定失败通常意味着参数下标越界、内存
// 不足或语句已失效；不在这里暴露，错误会拖到 step 阶段并伪装成别的故障。
void check_bind(int rc, sqlite3_stmt* stmt) {
    if (rc != SQLITE_OK) {
        throw std::runtime_error("绑定参数失败: " +
                                 std::string(sqlite3_errmsg(sqlite3_db_handle(stmt))));
    }
}

}  // namespace

Database::Database(const std::string& db_path) {
    // sqlite3_open 失败时也可能分配了句柄，必须 close 再抛，否则泄漏。
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err_msg = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("打开数据库失败: " + err_msg);
    }
}

Database::~Database() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

// 移动语义都是“接管指针 + 源对象置空”，源对象析构时不会关闭已转移的连接。
Database::Database(Database&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        // 先释放自己持有的连接，否则被覆盖后泄漏。
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

sqlite3* Database::get() const {
    return db_;
}

Statement::Statement(Database& db, const std::string& sql) {
    // nByte = -1：sql 以 NUL 结尾；末尾输出参数（未编译的尾巴）本项目用不到，传 nullptr。
    int rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("准备语句失败: " + std::string(sqlite3_errmsg(db.get())));
    }
}

Statement::~Statement() {
    if (stmt_ != nullptr) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_ != nullptr) {
            sqlite3_finalize(stmt_);
        }
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

void Statement::bind(int index, int value) {
    check_bind(sqlite3_bind_int(stmt_, index, value), stmt_);
}

void Statement::bind(int index, double value) {
    check_bind(sqlite3_bind_double(stmt_, index, value), stmt_);
}

void Statement::bind(int index, const std::string& value) {
    // SQLITE_TRANSIENT：让 SQLite 立即拷贝字符串内容。
    // 不用 SQLITE_STATIC，是因为调用方的 value 可能先于语句失效。
    check_bind(sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT), stmt_);
}

void Statement::bind_null(int index) {
    check_bind(sqlite3_bind_null(stmt_, index), stmt_);
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) {
        return true;
    } else if (rc == SQLITE_DONE) {
        return false;
    } else {
        // 其余返回码（约束冲突、IO 错误等）统一升级为异常，
        // 调用方不必逐个检查 C 风格返回码。
        throw std::runtime_error("执行语句失败: " +
                                 std::string(sqlite3_errmsg(sqlite3_db_handle(stmt_))));
    }
}

void Statement::reset() {
    // clear_bindings 不能省：reset 只复位执行状态，不会清理已绑定的参数。
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

int Statement::column_int(int col) const {
    return sqlite3_column_int(stmt_, col);
}
double Statement::column_double(int col) const {
    return sqlite3_column_double(stmt_, col);
}
std::string Statement::column_text(int col) const {
    // 列值为 SQL NULL（或内存不足）时返回 nullptr，这里统一折叠成空串。
    const unsigned char* text = sqlite3_column_text(stmt_, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

sqlite3_stmt* Statement::get() const {
    return stmt_;
}

}  // namespace storage