#include "db/database.h"
#include "utils/logger.h"
#include <iostream>

Database::Database(const std::string& db_path) : db_(nullptr), db_path_(db_path) {
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::init() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        Logger::instance().error("Cannot open database: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    Logger::instance().info("Database opened: " + db_path_);

    if (!create_table()) {
        return false;
    }

    return true;
}

bool Database::create_table() {
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            email TEXT NOT NULL,
            content TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";

    return execute_sql(sql);
}

bool Database::execute_sql(const std::string& sql) {
    char* err_msg;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        Logger::instance().error("SQL error: " + std::string(err_msg));
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

bool Database::add_message(const std::string& name, const std::string& email, const std::string& content) {
    std::string sql = "INSERT INTO messages (name, email, content) VALUES ('" + 
                      name + "', '" + email + "', '" + content + "');";

    return execute_sql(sql);
}

bool Database::get_messages(std::vector<Message>& messages) {
    messages.clear();

    std::string sql = "SELECT id, name, email, content, created_at FROM messages ORDER BY id DESC;";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        Logger::instance().error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int(stmt, 0);
        msg.name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        msg.email = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        msg.content = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        msg.created_at = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        messages.push_back(msg);
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        Logger::instance().error("Failed to execute query: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    return true;
}

bool Database::get_message(int id, Message& message) {
    std::string sql = "SELECT id, name, email, content, created_at FROM messages WHERE id = " + 
                      std::to_string(id) + ";";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        Logger::instance().error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        message.id = sqlite3_column_int(stmt, 0);
        message.name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        message.email = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        message.content = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        message.created_at = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
    } else {
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool Database::delete_message(int id) {
    std::string sql = "DELETE FROM messages WHERE id = " + std::to_string(id) + ";";
    return execute_sql(sql);
}

int Database::get_message_count() {
    std::string sql = "SELECT COUNT(*) FROM messages;";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        Logger::instance().error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return 0;
    }

    rc = sqlite3_step(stmt);
    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}