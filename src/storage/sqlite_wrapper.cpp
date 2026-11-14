#include "sqlite_wrapper.h"

#include <sqlite3.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace storage {

Database::Database(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err_msg = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open database: " + err_msg);
    }
}

Database::~Database() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

Database::Database(Database&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
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
    int rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " +
                                 std::string(sqlite3_errmsg(db.get())));
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
    sqlite3_bind_int(stmt_, index, value);
}

void Statement::bind(int index, double value) {
    sqlite3_bind_double(stmt_, index, value);
}

void Statement::bind(int index, const std::string& value) {
    sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void Statement::bind_null(int index) {
    sqlite3_bind_null(stmt_, index);
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) {
        return true;
    } else if (rc == SQLITE_DONE) {
        return false;
    } else {
        throw std::runtime_error("Failed to step statement: " +
                                 std::string(sqlite3_errmsg(sqlite3_db_handle(stmt_))));
    }
}

void Statement::reset() {
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
    const unsigned char* text = sqlite3_column_text(stmt_, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

sqlite3_stmt* Statement::get() const {
    return stmt_;
}

}  // namespace storage