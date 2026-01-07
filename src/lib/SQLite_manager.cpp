#include "SQLite_manager.h"
#include <iostream>

SQLiteManager::SQLiteManager(const string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        db = nullptr;
    }
}

SQLiteManager::~SQLiteManager() {
    if (db) sqlite3_close(db);
}

bool SQLiteManager::init_database() {
    
    const char* sql_logs = "CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp TEXT, hostname TEXT, facility TEXT, severity TEXT, application TEXT, message TEXT, pid TEXT, source_type TEXT, created_at DATETIME DEFAULT CURRENT_TIMESTAMP);";
    const char* sql_users = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE, password TEXT, role TEXT);";

    
    const char* sql_sources = R"(
        CREATE TABLE IF NOT EXISTS sources (
            ip TEXT PRIMARY KEY,
            status TEXT DEFAULT 'PENDING',
            last_activity INTEGER DEFAULT 0
        );
    )";
    
    sqlite3_exec(db, sql_logs, nullptr, nullptr, nullptr);
    sqlite3_exec(db, sql_users, nullptr, nullptr, nullptr);
    sqlite3_exec(db, sql_sources, nullptr, nullptr, nullptr);
    
    create_default_user();
    return true;
}

void SQLiteManager::create_default_user() {
    lock_guard<mutex> lock(db_mutex);
    sqlite3_exec(db, "INSERT OR IGNORE INTO users (username, password, role) VALUES ('admin', 'admin', 'ADMIN')", nullptr, nullptr, nullptr);
}

bool SQLiteManager::validate_user(const string& u, const string& p, string& r) { return true; } 

int SQLiteManager::insert_log(const string& timestamp, const string& hostname, const string& facility, const string& severity, const string& application, const string& message, const string& pid, const string& source_type) {
    lock_guard<mutex> lock(db_mutex);
    const char* sql = "INSERT INTO logs (timestamp, hostname, facility, severity, application, message, pid, source_type) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt; if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT); sqlite3_bind_text(stmt, 2, hostname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, facility.c_str(), -1, SQLITE_TRANSIENT); sqlite3_bind_text(stmt, 4, severity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, application.c_str(), -1, SQLITE_TRANSIENT); sqlite3_bind_text(stmt, 6, message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, pid.c_str(), -1, SQLITE_TRANSIENT); sqlite3_bind_text(stmt, 8, source_type.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(stmt); sqlite3_finalize(stmt); return (result == SQLITE_DONE) ? 1 : 0;
}

vector<LogEntry> SQLiteManager::search_logs(string keyword, string severity_filter, string limit) {
    lock_guard<mutex> lock(db_mutex); vector<LogEntry> results;
    string sql = "SELECT timestamp, hostname, pid, facility, severity, application, message FROM logs WHERE 1=1";
    if (!keyword.empty()) sql += " AND message LIKE '%" + keyword + "%'";
    if (severity_filter != "ALL" && !severity_filter.empty()) sql += " AND severity='" + severity_filter + "'";
    sql += " ORDER BY id DESC LIMIT " + (limit.empty() ? "100" : limit);
    sqlite3_stmt* stmt; if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            LogEntry e; e.timestamp=(const char*)sqlite3_column_text(stmt,0); e.hostname=(const char*)sqlite3_column_text(stmt,1);
            e.pid=(const char*)sqlite3_column_text(stmt,2); e.facility=(const char*)sqlite3_column_text(stmt,3);
            e.severity=(const char*)sqlite3_column_text(stmt,4); e.app=(const char*)sqlite3_column_text(stmt,5);
            e.message=(const char*)sqlite3_column_text(stmt,6); results.push_back(e);
        }
    } sqlite3_finalize(stmt); return results;
}

map<string, int> SQLiteManager::get_severity_counts() {
    lock_guard<mutex> lock(db_mutex); map<string, int> c;
    sqlite3_stmt* s; if(sqlite3_prepare_v2(db,"SELECT severity, COUNT(*) FROM logs GROUP BY severity",-1,&s,0)==SQLITE_OK) 
    while(sqlite3_step(s)==SQLITE_ROW) c[(const char*)sqlite3_column_text(s,0)] = sqlite3_column_int(s,1);
    sqlite3_finalize(s); return c;
}

vector<pair<string, int>> SQLiteManager::get_top_sources() {
    lock_guard<mutex> lock(db_mutex); vector<pair<string, int>> r;
    sqlite3_stmt* s; if(sqlite3_prepare_v2(db,"SELECT hostname, COUNT(*) as c FROM logs GROUP BY hostname ORDER BY c DESC LIMIT 5",-1,&s,0)==SQLITE_OK)
    while(sqlite3_step(s)==SQLITE_ROW) r.push_back({(const char*)sqlite3_column_text(s,0), sqlite3_column_int(s,1)});
    sqlite3_finalize(s); return r;
}

vector<string> SQLiteManager::query_logs(const string&, int) { return {}; }



void SQLiteManager::register_or_update_source(const string& ip, const string& status) {
    lock_guard<mutex> lock(db_mutex);
    long long now = time(nullptr);

    if (status == "PENDING") {
        
        const char* sql_insert = "INSERT OR IGNORE INTO sources (ip, status, last_activity) VALUES (?, 'PENDING', ?)";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 2, now);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        
        
        const char* sql_update = "UPDATE sources SET last_activity = ? WHERE ip = ?";
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, now);
            sqlite3_bind_text(stmt, 2, ip.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    } 
    else {
        
        const char* sql = "INSERT INTO sources (ip, status, last_activity) VALUES (?, ?, ?) "
                          "ON CONFLICT(ip) DO UPDATE SET status=excluded.status, last_activity=excluded.last_activity";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, status.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 3, now);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    }
}

void SQLiteManager::update_heartbeat(const string& ip) {
    lock_guard<mutex> lock(db_mutex);
    long long now = time(nullptr);
    const char* sql = "UPDATE sources SET last_activity = ? WHERE ip = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, now);
        sqlite3_bind_text(stmt, 2, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
}

vector<AgentSource> SQLiteManager::get_all_sources() {
    lock_guard<mutex> lock(db_mutex);
    vector<AgentSource> list;
    const char* sql = "SELECT ip, status, last_activity FROM sources";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            AgentSource s;
            s.ip = (const char*)sqlite3_column_text(stmt, 0);
            s.status = (const char*)sqlite3_column_text(stmt, 1);
            s.last_activity = sqlite3_column_int64(stmt, 2);
            list.push_back(s);
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

bool SQLiteManager::is_source_blocked(const string& ip) {
    lock_guard<mutex> lock(db_mutex);
    bool blocked = true; 
    const char* sql = "SELECT status FROM sources WHERE ip = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            string status = (const char*)sqlite3_column_text(stmt, 0);
            if (status == "ACTIVE") blocked = false;
        }
    }
    sqlite3_finalize(stmt);
    return blocked;
}
