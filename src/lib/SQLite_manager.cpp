// sqlite_manager.cpp
#include "SQLite_manager.h"
#include <iostream>

SQLiteManager::SQLiteManager(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
    }
}

SQLiteManager::~SQLiteManager() {
    sqlite3_close(db);
}

bool SQLiteManager::init_database() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            hostname TEXT,
            severity TEXT,
            application TEXT,
            message TEXT,
            pid TEXT,
            source_type TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS idx_timestamp ON logs(timestamp);
        CREATE INDEX IF NOT EXISTS idx_severity ON logs(severity);
        CREATE INDEX IF NOT EXISTS idx_hostname ON logs(hostname);
    )";
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

int SQLiteManager::insert_log(const std::string& timestamp,
                                   const std::string& hostname,
                                   const std::string& severity,
                                   const std::string& application,
                                   const std::string& message,
                                   const std::string& pid,
                                   const std::string& source_type) {
    std::lock_guard<std::mutex> lock(db_mutex);
    
    const char* sql = R"(
        INSERT INTO logs (timestamp, hostname, severity, application, message, pid, source_type)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hostname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, severity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, application.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, pid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, source_type.c_str(), -1, SQLITE_TRANSIENT);
    
    int result = sqlite3_step(stmt);
    int last_id = -1;
    
    if (result == SQLITE_DONE) {
        last_id = sqlite3_last_insert_rowid(db);
    } else {
        std::cerr << "Failed to insert: " << sqlite3_errmsg(db) << std::endl;
    }
    
    sqlite3_finalize(stmt);
    return last_id;
}

std::vector<LogEntry> SQLiteManager::search_logs(std::string keyword, std::string severity_filter, std::string limit) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<LogEntry> results;
    
    std::string sql = "SELECT timestamp, hostname, pid, severity, application, message FROM logs WHERE 1=1";

    // Adăugare filtre dinamic
    if (!keyword.empty()) {
        sql += " AND message LIKE '%" + keyword + "%'";
    }
    if (severity_filter != "ALL" && !severity_filter.empty()) {
        sql += " AND severity='" + severity_filter + "'";
    }
    
    // Sortare și limitare
    sql += " ORDER BY id DESC LIMIT " + (limit.empty() ? "100" : limit);

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            LogEntry entry;
            // Verificăm dacă coloana nu este NULL înainte de cast
            const char* ts = (const char*)sqlite3_column_text(stmt, 0);
            const char* host = (const char*)sqlite3_column_text(stmt, 1);
            const char* pid = (const char*)sqlite3_column_text(stmt, 2);
            const char* sev = (const char*)sqlite3_column_text(stmt, 3);
            const char* app = (const char*)sqlite3_column_text(stmt, 4);
            const char* msg = (const char*)sqlite3_column_text(stmt, 5);

            entry.timestamp = ts ? ts : "";
            entry.hostname  = host ? host : "";
            entry.pid       = pid ? pid : "";
            entry.severity  = sev ? sev : "";
            entry.app       = app ? app : "";
            entry.message   = msg ? msg : "";
            
            results.push_back(entry);
        }
    } else {
        std::cerr << "[DB Error] Search failed: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
    return results;
}