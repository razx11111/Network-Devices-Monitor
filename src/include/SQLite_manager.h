#ifndef SQLITE_MANAGER_H
#define SQLITE_MANAGER_H

#include <sqlite3.h>
#include <string>
#include <mutex>
#include <vector>
#include <map> // <--- CRITIC: Necesar pentru std::map

// Structură pentru rezultatele căutării
struct LogEntry {
    std::string timestamp;
    std::string hostname;
    std::string pid;
    std::string facility; 
    std::string severity;
    std::string app;
    std::string message;
};

class SQLiteManager {
private:
    sqlite3* db;
    std::mutex db_mutex;
    
public:
    SQLiteManager(const std::string& db_path);
    ~SQLiteManager();
    
    bool init_database();
    int insert_log(const std::string& timestamp,
                       const std::string& hostname,
                       const std::string& facility,
                       const std::string& severity,
                       const std::string& application,
                       const std::string& message,
                       const std::string& pid,
                       const std::string& source_type);
    
    // Funcția de căutare
    std::vector<LogEntry> search_logs(std::string query, std::string severity, std::string limit);

    std::map<std::string, int> get_severity_counts();
    
    // Query-uri generale
    std::vector<std::string> query_logs(const std::string& where_clause, int limit);
};

#endif