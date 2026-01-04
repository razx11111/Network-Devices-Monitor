#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>

#include "protocol.h"
#include "tcp_server_func.h"
#include "SQLite_manager.h"

using namespace std;

extern SQLiteManager* g_db_manager;

// Lista globală de dashboard-uri
vector<int> dashboard_sockets;
pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dlock = PTHREAD_MUTEX_INITIALIZER; 

// --- JSON ESCAPE FUNCTION (CRITIC PENTRU SEARCH) ---
// Previne stricarea JSON-ului dacă mesajul conține ghilimele sau enter-uri
string json_escape(const string& str) {
    string output;
    for (char c : str) {
        switch (c) {
            case '\"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    // Ignorăm caracterele de control
                } else {
                    output += c;
                }
        }
    }
    return output;
}
// ---------------------------------------------------

void broadcast_to_dashboards(string jsonLog) {
    pthread_mutex_lock(&dlock);
    
    AMPHeader header;
    header.version = 1;
    header.message_type = CMD_LOG; 
    header.reserved = 0;
    header.payload_length = htonl(jsonLog.size()); 

    for (size_t i = 0; i < dashboard_sockets.size(); i++) {
        // Trimitem header
        send(dashboard_sockets[i], &header, sizeof(AMPHeader), MSG_NOSIGNAL);
        // Trimitem payload
        send(dashboard_sockets[i], jsonLog.c_str(), jsonLog.size(), MSG_NOSIGNAL);
    }
    pthread_mutex_unlock(&dlock);
}

// FIX: Parsare robustă a JSON-ului pentru a găsi cheile corect
static string extract_field(const string& json, const string& key) {
    // Căutăm: "key":
    string pattern = "\"" + key + "\":";
    auto pos = json.find(pattern);
    
    // Dacă nu găsim exact "key":, încercăm și cu spații "key" :
    if (pos == string::npos) {
        pattern = "\"" + key + "\"";
        pos = json.find(pattern);
        if (pos == string::npos) return ""; // Cheia nu există
        
        // Găsim două puncte după cheie
        pos = json.find(':', pos);
        if (pos == string::npos) return "";
    } else {
        // Am găsit pattern-ul exact, sărim peste el
        pos += pattern.size(); 
        // Ajustăm dacă pattern-ul era doar cheia fără două puncte (cazul else de sus)
        if (json[pos-1] != ':') pos = json.find(':', pos) + 1;
    }

    // Sărim peste spații
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }

    if (pos >= json.size()) return "";

    string value;
    if (json[pos] == '"') {
        // Este un string "valoare"
        pos++; // Sărim peste ghilimeaua de start
        while (pos < json.size()) {
            if (json[pos] == '"' && json[pos-1] != '\\') break; // Ghilimea de final
            value += json[pos];
            pos++;
        }
    } else {
        // Este număr sau altceva (fără ghilimele)
        while (pos < json.size() && json[pos] != ',' && json[pos] != '}') {
            value += json[pos];
            pos++;
        }
    }
    return value;
}

void *treat(void *arg) {
    struct thData tdL;
    tdL = *((struct thData *)arg);
    printf("[thread %d] Client connected.\n", tdL.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());
    
    raspunde((struct thData *)arg);
    
    // Cleanup: Scoatem dashboard-ul din listă dacă se deconectează
    pthread_mutex_lock(&dlock);
    for (auto it = dashboard_sockets.begin(); it != dashboard_sockets.end(); ) {
        if (*it == tdL.cl) {
            cout << "[Thread " << tdL.idThread << "] Unregistering Dashboard socket " << tdL.cl << endl;
            it = dashboard_sockets.erase(it);
        } else {
            ++it;
        }
    }
    pthread_mutex_unlock(&dlock);

    close(tdL.cl);
    free(arg);
    return (NULL);
}

void raspunde(void *arg) {
    struct thData tdL = *((struct thData *)arg);
    AMPHeader header;
    
    for ( ; ; ) {
        if (!read_n_bytes(tdL.cl, &header, sizeof(AMPHeader))) break;

        u32 payloadLen = ntohl(header.payload_length);
        char* payload = new char[payloadLen + 1];
        if (payloadLen > 0) {
            if (!read_n_bytes(tdL.cl, payload, payloadLen)) {
                delete[] payload;
                break;
            }
        }
        payload[payloadLen] = '\0';
        string payloadStr(payload, payloadLen);
        string responseMsg;

        pthread_mutex_lock(&mlock);
        
        switch (header.message_type) {
            case CMD_AUTH:
                cout << "[Thread " << tdL.idThread << "] AUTH_REQ: " << payloadStr << endl;
                if (payloadStr.find("ADMIN") != string::npos) {
                    pthread_mutex_lock(&dlock);
                    dashboard_sockets.push_back(tdL.cl);
                    pthread_mutex_unlock(&dlock);
                    cout << "[Thread " << tdL.idThread << "] -> Registered as DASHBOARD." << endl;
                }
                responseMsg = "{\"status\":\"ok\",\"cmd\":\"AUTH_REQ\"}";
                break;

            case CMD_LOG:
                {
                    string timestamp = extract_field(payloadStr, "timestamp");
                    string hostname = extract_field(payloadStr, "hostname");
                    string facility = extract_field(payloadStr, "facility");
                    if (facility.empty()) facility = "USER";
                    string severity = extract_field(payloadStr, "severity");
                    string app = extract_field(payloadStr, "application");
                    string msg = extract_field(payloadStr, "message");
                    string pid = extract_field(payloadStr, "pid");

                    if (app.empty()) app = "System";

                    g_db_manager->insert_log(timestamp, hostname, facility, severity, app, msg, pid, "agent");
                    cout << "[Thread " << tdL.idThread << "] LOG_DATA saved." << endl;
                    
                    pthread_mutex_unlock(&mlock); 
                    broadcast_to_dashboards(payloadStr);
                    pthread_mutex_lock(&mlock); 
                }
                responseMsg = "{\"status\":\"ok\",\"cmd\":\"LOG_DATA\"}";
                break;
            
            case CMD_SEARCH:
            {
                cout << "[Thread " << tdL.idThread << "] SEARCH_REQ: " << payloadStr << endl;
                
                string keyword = extract_field(payloadStr, "keyword");
                string sev     = extract_field(payloadStr, "severity");
                string limit   = extract_field(payloadStr, "limit");

                // 1. Query DB
                vector<LogEntry> logs = g_db_manager->search_logs(keyword, sev, limit);

                // 2. Build JSON Response
                // Folosim json_escape pentru a nu strica formatul!
                string jsonResp = "{\"status\":\"ok\",\"results\":[";
                for (size_t i = 0; i < logs.size(); ++i) {
                    jsonResp += "{";
                    jsonResp += "\"timestamp\":\"" + json_escape(logs[i].timestamp) + "\",";
                    jsonResp += "\"hostname\":\"" + json_escape(logs[i].hostname) + "\",";
                    jsonResp += "\"pid\":\"" + json_escape(logs[i].pid) + "\",";
                    jsonResp += "\"facility\":\"" + json_escape(logs[i].facility) + "\",";
                    jsonResp += "\"severity\":\"" + json_escape(logs[i].severity) + "\",";
                    jsonResp += "\"application\":\"" + json_escape(logs[i].app) + "\",";
                    jsonResp += "\"message\":\"" + json_escape(logs[i].message) + "\"";
                    jsonResp += "}";
                    if (i < logs.size() - 1) jsonResp += ",";
                }
                jsonResp += "]}";

                responseMsg = jsonResp;
                break;
            }

            case CMD_STATS:
            {
                cout << "[Thread " << tdL.idThread << "] STATS_REQ" << endl;
                
                // 1. Get Counts
                auto counts = g_db_manager->get_severity_counts();
                
                // 2. Build JSON
                // Format: {"status":"ok", "stats": {"INFO": 10, "ERROR": 2}}
                string json = "{\"status\":\"ok\",\"stats\":{";
                int i = 0;
                for (auto const& [sev, count] : counts) {
                    json += "\"" + sev + "\":" + to_string(count);
                    if (i < counts.size() - 1) json += ",";
                    i++;
                }
                json += "}}";
                
                responseMsg = json;
                break;
            }

            case CMD_HEARTBEAT:
                responseMsg = "{\"status\":\"ok\",\"cmd\":\"HEARTBEAT\"}";
                break;
            
            default:
                responseMsg = "{\"status\":\"error\",\"cmd\":\"UNKNOWN\"}";
                break;
        }
        pthread_mutex_unlock(&mlock);
        delete[] payload;

        AMPHeader respHeader;
        respHeader.version = 1;
        respHeader.message_type = header.message_type;
        respHeader.reserved = 0;
        respHeader.payload_length = htonl(responseMsg.size());

        if (write(tdL.cl, &respHeader, sizeof(respHeader)) <= 0) break;
        if (write(tdL.cl, responseMsg.data(), responseMsg.size()) <= 0) break;
    }
}

bool read_n_bytes(int socket, void* buffer, int n) {
    int totalBytesRead = 0;
    char* p = (char*)buffer;
    while (totalBytesRead < n) {
        int bytesRead = read(socket, p + totalBytesRead, n - totalBytesRead);
        if (bytesRead <= 0) return false; 
        totalBytesRead += bytesRead;
    }
    return true;
}