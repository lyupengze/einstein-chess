#ifndef SQLITE_WRAPPER_H_
#define SQLITE_WRAPPER_H_

#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace storage {

class Database {
public:
    explicit Database(const std::string& db_path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    sqlite3* get() const;

private:
    sqlite3* db_ = nullptr;
};

class Statement {
public:
    Statement(Database& db, const std::string& sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    void bind(int index, int value);
    void bind(int index, double value);
    void bind(int index, const std::string& value);
    void bind_null(int index);

    bool step();

    void reset();

    int column_int(int col) const;
    double column_double(int col) const;
    std::string column_text(int col) const;

    sqlite3_stmt* get() const;

private:
    sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace storage

#endif