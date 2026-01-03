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
#include <algorithm> // Required for cleanup
#include <vector>    // Required for vector

#include "protocol.h"
#include "tcp_server_func.h"
#include "SQLite_manager.h"

using namespace std;

extern SQLiteManager* g_db_manager;

// Global list of dashboard sockets
vector<int> dashboard_sockets;
pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dlock = PTHREAD_MUTEX_INITIALIZER; // Protects dashboard_sockets

// Helper to safely send to all dashboards
void broadcast_to_dashboards(string jsonLog) {
    pthread_mutex_lock(&dlock);
    
    // 1. Construct the Header
    AMPHeader header;
    header.version = 1;
    header.message_type = CMD_LOG; // Tell dashboard this is a log
    header.reserved = 0;
    header.payload_length = htonl(jsonLog.size()); // Critical: Network Byte Order

    // Loop through all dashboards
    for (int i = 0; i < dashboard_sockets.size(); i++) {
        // 2. Send Header First
        send(dashboard_sockets[i], &header, sizeof(AMPHeader), MSG_NOSIGNAL);
        
        // 3. Send Payload (JSON)
        send(dashboard_sockets[i], jsonLog.c_str(), jsonLog.size(), MSG_NOSIGNAL);
    }
    pthread_mutex_unlock(&dlock);
}

// Helper: Extract JSON value manually (since we don't use a JSON lib here)
static string extract_field(const string& json, const string& key) {
    const string pattern = "\"" + key + "\":"; // Look for "key":
    auto pos = json.find(pattern);
    if (pos == string::npos) {
        // Retry with space "key" :
        pos = json.find("\"" + key + "\" :");
        if (pos == string::npos) return "";
    }
    
    // Move past the key and colon
    pos = json.find(':', pos) + 1;

    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '"')) {
        pos++;
    }

    string value;
    while (pos < json.size() && json[pos] != '"' && json[pos] != '}' && json[pos] != ',') {
        value.push_back(json[pos]);
        pos++;
    }
    return value;
}

void *treat(void *arg) {
    struct thData tdL;
    tdL = *((struct thData *)arg);
    printf("[thread %d] Client connected. Waiting for commands...\n", tdL.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());
    
    raspunde((struct thData *)arg);
    
    // If this client was a dashboard, remove it from the list
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
                
                // --- NEW LOGIC: Check for ADMIN Role ---
                if (payloadStr.find("ADMIN") != string::npos) {
                    pthread_mutex_lock(&dlock);
                    dashboard_sockets.push_back(tdL.cl);
                    pthread_mutex_unlock(&dlock);
                    cout << "[Thread " << tdL.idThread << "] -> Registered as DASHBOARD." << endl;
                }
                // ---------------------------------------
                
                responseMsg = "{\"status\":\"ok\",\"cmd\":\"AUTH_REQ\"}";
                break;

            case CMD_LOG:
                {
                    // 1. Save to DB
                    string timestamp = extract_field(payloadStr, "timestamp");
                    string hostname = extract_field(payloadStr, "hostname");
                    string severity = extract_field(payloadStr, "severity");
                    string app = extract_field(payloadStr, "application");
                    string msg = extract_field(payloadStr, "message");
                    string pid = extract_field(payloadStr, "PID");

                    // Handle missing application field in legacy logs
                    if (app.empty()) app = "System";

                    g_db_manager->insert_log(timestamp, hostname, severity, app, msg, pid, "agent");
                    cout << "[Thread " << tdL.idThread << "] LOG_DATA saved." << endl;
                    
                    // 2. Broadcast to Dashboards
                    // We must release mlock before broadcasting to avoid deadlocks with dlock
                    pthread_mutex_unlock(&mlock); 
                    broadcast_to_dashboards(payloadStr);
                    pthread_mutex_lock(&mlock); // Relock for the loop end
                }
                responseMsg = "{\"status\":\"ok\",\"cmd\":\"LOG_DATA\"}";
                break;

            case CMD_HEARTBEAT:
                responseMsg = "{\"status\":\"ok\",\"cmd\":\"HEARTBEAT\"}";
                break;
        }
        pthread_mutex_unlock(&mlock);
        delete[] payload;

        // Send Response
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