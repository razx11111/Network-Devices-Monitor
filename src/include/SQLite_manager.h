#ifndef SQLITE_MANAGER_H
#define SQLITE_MANAGER_H

#include <sqlite3.h>
#include <string>
#include <mutex>
#include <vector>
#include <map> 

using namespace std;

struct AgentSource {
    string ip;
    string status; 
    long long last_activity;
};

struct LogEntry {
    string timestamp;
    string hostname;
    string pid;
    string facility; 
    string severity;
    string app;
    string message;
};

class SQLiteManager {
private:
    sqlite3* db;
    mutex db_mutex;
    
public:
    SQLiteManager(const string& db_path);
    ~SQLiteManager();
    
    bool init_database();
    int insert_log(const string& timestamp,
                       const string& hostname,
                       const string& facility,
                       const string& severity,
                       const string& application,
                       const string& message,
                       const string& pid,
                       const string& source_type);
    
    vector<LogEntry> search_logs(string query, string severity, string limit);
    map<string, int> get_severity_counts();
    vector<pair<string, int>> get_top_sources();
    vector<string> query_logs(const string& where_clause, int limit);

    bool validate_user(const string& username, const string& password, string& role);
    void create_default_user(); 
    
    void register_or_update_source(const string& ip, const string& status);
    
    
    void update_heartbeat(const string& ip);
    vector<AgentSource> get_all_sources();
    bool is_source_blocked(const string& ip);
};

#endif
