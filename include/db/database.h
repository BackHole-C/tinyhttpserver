#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <sqlite3.h>

struct Message {
    int id;
    std::string name;
    std::string email;
    std::string content;
    std::string created_at;
};

class Database {
public:
    Database(const std::string& db_path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool init();
    bool add_message(const std::string& name, const std::string& email, const std::string& content);
    bool get_messages(std::vector<Message>& messages);
    bool get_message(int id, Message& message);
    bool delete_message(int id);
    int get_message_count();

private:
    sqlite3* db_;
    std::string db_path_;

    bool execute_sql(const std::string& sql);
    bool create_table();
};

#endif // DATABASE_H