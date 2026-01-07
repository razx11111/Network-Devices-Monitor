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
#include <vector>
#include "protocol.h"
#include "tcp_server_func.h"
#include "SQLite_manager.h"
#include <thread> 
#include <chrono>

using namespace std;

extern SQLiteManager* g_db_manager;
vector<int> dashboard_sockets;
pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dlock = PTHREAD_MUTEX_INITIALIZER;

string json_escape(const string& str) {
    string output;
    for (char c : str) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default: output += c;
        }
    }
    return output;
}

void broadcast_to_dashboards(string jsonLog) {
    pthread_mutex_lock(&dlock);
    AMPHeader header;
    header.version = 1;
    header.message_type = CMD_LOG; 
    header.reserved = 0;
    header.payload_length = htonl(jsonLog.size()); 
    for (size_t i = 0; i < dashboard_sockets.size(); i++) {
        send(dashboard_sockets[i], &header, sizeof(AMPHeader), MSG_NOSIGNAL);
        send(dashboard_sockets[i], jsonLog.c_str(), jsonLog.size(), MSG_NOSIGNAL);
    }
    pthread_mutex_unlock(&dlock);
}

static string extract_field(const string& json, const string& key) {
    
    string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    
    if (keyPos == string::npos) return ""; 

    
    size_t colonPos = json.find(':', keyPos + searchKey.length());
    if (colonPos == string::npos) return "";

    
    size_t startValue = colonPos + 1;
    while (startValue < json.length() && (json[startValue] == ' ' || json[startValue] == '\t' || json[startValue] == '\n')) {
        startValue++;
    }
    if (startValue >= json.length()) return "";

    
    string value;
    if (json[startValue] == '"') {
        
        startValue++; 
        size_t endValue = startValue;
        while (endValue < json.length()) {
            if (json[endValue] == '"' && json[endValue - 1] != '\\') break; 
            endValue++;
        }
        value = json.substr(startValue, endValue - startValue);
    } else {
        
        size_t endValue = startValue;
        while (endValue < json.length() && json[endValue] != ',' && json[endValue] != '}') {
            endValue++;
        }
        value = json.substr(startValue, endValue - startValue);
    }
    
    return value;
}

void *treat(void *arg) {
    struct thData tdL = *((struct thData *)arg);
    pthread_detach(pthread_self());
    raspunde((void*)arg);
    pthread_mutex_lock(&dlock);
    for (auto it = dashboard_sockets.begin(); it != dashboard_sockets.end(); ) {
        if (*it == tdL.cl) it = dashboard_sockets.erase(it);
        else ++it;
    }
    pthread_mutex_unlock(&dlock);
    close(tdL.cl);
    free(arg);
    return (NULL);
}

void run_simulation(string hostname);

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
            {
                string pass = extract_field(payloadStr, "password");
                string role = (pass == "admin") ? "ADMIN" : "VIEWER";
                pthread_mutex_lock(&dlock);
                dashboard_sockets.push_back(tdL.cl);
                pthread_mutex_unlock(&dlock);
                responseMsg = "{\"status\":\"ok\",\"role\":\"" + role + "\"}";
                break;
            }

            case CMD_LOG:
            {
                string timestamp = extract_field(payloadStr, "timestamp");
                string hostname = extract_field(payloadStr, "hostname");
                
                
                g_db_manager->register_or_update_source(hostname, "PENDING");
                g_db_manager->update_heartbeat(hostname);
                
                
                if (g_db_manager->is_source_blocked(hostname)) {
                    responseMsg = "{\"status\":\"error\",\"message\":\"Blocked\"}";
                } else {
                    string facility = extract_field(payloadStr, "facility"); 
                    if (facility.empty()) facility = "USER"; 
                    string severity = extract_field(payloadStr, "severity");
                    string app = extract_field(payloadStr, "application");
                    string msg = extract_field(payloadStr, "message");
                    string pid = extract_field(payloadStr, "pid");
                    if (app.empty()) app = "System";

                    g_db_manager->insert_log(timestamp, hostname, facility, severity, app, msg, pid, "agent");
                    pthread_mutex_unlock(&mlock); 
                    broadcast_to_dashboards(payloadStr);
                    pthread_mutex_lock(&mlock); 
                    responseMsg = "{\"status\":\"ok\"}";
                }
                break;
            }
            
            case CMD_SEARCH:
            {
                string keyword = extract_field(payloadStr, "keyword");
                string sev = extract_field(payloadStr, "severity");
                string limit = extract_field(payloadStr, "limit");
                vector<LogEntry> logs = g_db_manager->search_logs(keyword, sev, limit);
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
                auto counts = g_db_manager->get_severity_counts();
                auto sources = g_db_manager->get_top_sources();
                string jsonResp = "{\"status\":\"ok\",\"stats\":{";
                for (auto const& [sev, count] : counts) {
                    jsonResp += "\"" + sev + "\":" + to_string(count) + ",";
                }
                jsonResp += "\"top_sources\":[";
                for (size_t j = 0; j < sources.size(); ++j) {
                    jsonResp += "{\"name\":\"" + json_escape(sources[j].first) + "\",";
                    jsonResp += "\"count\":" + to_string(sources[j].second) + "}";
                    if (j < sources.size() - 1) jsonResp += ",";
                }
                jsonResp += "]";
                jsonResp += "}}";
                responseMsg = jsonResp;
                break;
            }
            case CMD_GET_AGENTS: 
            {
                cout << "[DEBUG] SERVER: Processing CMD_GET_AGENTS..." << endl; 
                auto list = g_db_manager->get_all_sources();
                
                string jsonResp = "{\"status\":\"ok\",\"agents\":[";
                long long now = time(nullptr);
                for (size_t i = 0; i < list.size(); ++i) {
                    long long diff = now - list[i].last_activity;
                    if (diff < 0) diff = 0;
                    string seenStr = (list[i].last_activity == 0) ? "Never" : to_string(diff) + "s ago";

                    jsonResp += "{\"ip\":\"" + list[i].ip + "\",";
                    jsonResp += "\"status\":\"" + list[i].status + "\",";
                    jsonResp += "\"last_seen\":\"" + seenStr + "\"}";
                    if (i < list.size() - 1) jsonResp += ",";
                }
                jsonResp += "]}";
                
                cout << "[DEBUG] SERVER: Sending Agent List: " << jsonResp << endl; 
                responseMsg = jsonResp;
                break;
            }

            case CMD_UPDATE_AGENT: 
            {
                string ip = extract_field(payloadStr, "ip");
                string status = extract_field(payloadStr, "status");
                
                if(!ip.empty()) {
                    
                    g_db_manager->register_or_update_source(ip, status);
                    responseMsg = "{\"status\":\"ok\"}";
                }
                break;
            }
            case CMD_ADD_AGENT: 
            {
                cout << "[DEBUG] RAW PAYLOAD RECEIVED: " << payloadStr << endl; 

                string ip = extract_field(payloadStr, "ip");
                string type = extract_field(payloadStr, "type"); 

                cout << "[DEBUG] EXTRACTED -> IP: '" << ip << "' | TYPE: '" << type << "'" << endl;

                if (ip.empty()) {
                    cout << "[ERROR] IP Extraction failed! Agent not added." << endl;
                    responseMsg = "{\"status\":\"error\",\"message\":\"Invalid IP\"}";
                } else {
                    
                    g_db_manager->register_or_update_source(ip, "ACTIVE");
                    cout << "[SUCCESS] Added agent to DB: " << ip << endl;
                    
                    if (type == "VIRTUAL") {
                        thread simThread(run_simulation, ip); 
                        simThread.detach(); 
                        cout << "[SIMULATION] Thread started for " << ip << endl;
                    }
                    responseMsg = "{\"status\":\"ok\"}";
                }
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

void run_simulation(string hostname) {
    cout << "[SIMULATION] STARTED for agent: " << hostname << endl;
    
    
    vector<string> messages = {
        "User admin logged in successfully via SSH",
        "Failed password for invalid user root from 192.168.1.100",
        "Connection closed by authenticating user",
        "System uptime is 14 days",
        "Disk usage at 85% on /dev/sda1",
        "Network interface eth0 link is UP",
        "Cron job /etc/cron.daily/backup executed",
        "Firewall: Blocked incoming connection on port 23"
    };

    vector<string> severities = {"INFO", "WARNING", "INFO", "NOTICE", "WARNING", "INFO", "INFO", "ALERT"};
    vector<string> facilities = {"AUTH", "AUTH", "AUTH", "SYSTEM", "SYSTEM", "KERNEL", "CRON", "SECURITY"};

    while (true) {

        if (g_db_manager->is_source_blocked(hostname)) { 
            this_thread::sleep_for(chrono::seconds(5)); continue; 
        }
        
        g_db_manager->update_heartbeat(hostname);

        
        
        bool blocked = g_db_manager->is_source_blocked(hostname);
        if (blocked) {
            cout << "[SIMULATION] Agent " << hostname << " is BLOCKED. Pausing simulation..." << endl;
            
            this_thread::sleep_for(chrono::seconds(5));
            continue;
        }

        
        int idx = rand() % messages.size();
        string msg = messages[idx];
        string sev = severities[idx];
        string fac = facilities[idx];
        
        
        time_t now = time(0);
        char ts[64];
        strftime(ts, sizeof(ts), "%b %d %H:%M:%S", localtime(&now));
        
        
        
        g_db_manager->insert_log(ts, hostname, fac, sev, "SimAgent", msg, "1337", "simulation");
        
        
        string jsonLog = "{";
        jsonLog += "\"timestamp\":\"" + string(ts) + "\",";
        jsonLog += "\"hostname\":\"" + hostname + "\",";
        jsonLog += "\"pid\":\"1337\",";
        jsonLog += "\"facility\":\"" + fac + "\",";
        jsonLog += "\"severity\":\"" + sev + "\",";
        jsonLog += "\"application\":\"SimAgent\",";
        jsonLog += "\"message\":\"" + msg + "\""; 
        jsonLog += "}"; 

        broadcast_to_dashboards(jsonLog);
        
        cout << "[SIMULATION] Generated log for " << hostname << ": " << msg << endl;

        this_thread::sleep_for(chrono::seconds(3));
    }
}