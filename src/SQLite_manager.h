#ifndef SQLITE_MANAGER_H
#define SQLITE_MANAGER_H

#include <sqlite3.h>
#include <string>
#include <mutex>
#include <vector>

class SQLiteManager {
private:
    sqlite3* db;
    std::mutex db_mutex;
    
public:
    SQLiteManager(const std::string& db_path);
    ~SQLiteManager();
    
    bool init_database();
    int64_t insert_log(const std::string& timestamp,
                       const std::string& hostname,
                       const std::string& severity,
                       const std::string& application,
                       const std::string& message,
                       const std::string& pid,
                       const std::string& source_type); // "agent" sau "syslog"
    
    // Pentru query-uri mai târziu
    std::vector<std::string> query_logs(const std::string& where_clause, int limit);
};

#endif