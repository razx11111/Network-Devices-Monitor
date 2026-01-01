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
#include "protocol.h"
#include "tcp_server_func.h"
#include "SQLite_manager.h"

extern SQLiteManager* g_db_manager;
vector<int> dashboard_sockets;
pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dlock = PTHREAD_MUTEX_INITIALIZER;

void broadcast_to_dashboards(string jsonLog) {
    pthread_mutex_lock(&dlock);
    // Loop through all dashboards and send the log
    for (int i = 0; i < dashboard_sockets.size(); i++) {
        send(dashboard_sockets[i], jsonLog.c_str(), jsonLog.size(), 0); 
    }
    pthread_mutex_unlock(&dlock);
}

static string extract_field(const string& json, const string& key) {
    const string pattern = "\"" + key + ":";
    auto pos = json.find(pattern);
    if (pos == string::npos) return "";
    pos += pattern.size();

    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '"')) {
        if (json[pos] == '"') { pos++; break; }
        pos++;
    }

    string value;
    while (pos < json.size() && json[pos] != '"') {
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
    
    close(tdL.cl);
    free(arg);
    return (NULL);
}

void raspunde(void *arg) {
    struct thData tdL = *((struct thData *)arg);
    AMPHeader header;
    int i=0;
    for ( ; ; ) {

        if (!read_n_bytes(tdL.cl, &header, sizeof(AMPHeader))) {
            printf("[Thread %d] Client disconnected or error reading header.\n", tdL.idThread);
            break;
        }

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
        string cmdName;
        
        pthread_mutex_lock(&mlock); 
        cout << "[Thread " << tdL.idThread << "] Command Received: ";
        
        switch (header.message_type) {
            case CMD_AUTH:
                cmdName = "AUTH_REQ";
                cout << cmdName << " | Payload: " << payload << endl;
                responseMsg = "{\"status\":\"ok\",\"cmd\":\"AUTH_REQ\"}";
                break;
            case CMD_LOG:
                cmdName = "LOG_DATA";
                cout << cmdName << " | Payload saved to database" << endl;
                
                {
                    string timestamp = extract_field(payloadStr, "timestamp");
                    string hostname = extract_field(payloadStr, "hostname");
                    string severity = extract_field(payloadStr, "severity");
                    string application = extract_field(payloadStr, "application");
                    string message = extract_field(payloadStr, "message");
                    string PID = extract_field(payloadStr, "PID");
                    
                    int64_t log_id = g_db_manager->insert_log(
                        timestamp, hostname, severity, application, message, PID, "agent"
                    );
                    
                    cout << "[Thread " << tdL.idThread << "] Log inserted with ID: " << log_id << endl;
                }
                broadcast_to_dashboards(payloadStr); 
                responseMsg = "{\"status\":\"ok\",\"cmd\":\"LOG_DATA\"}";
                break;
            case CMD_HEARTBEAT:
                cmdName = "HEARTBEAT";
                cout << cmdName << endl;
                responseMsg = "{\"status\":\"ok\",\"cmd\":\"HEARTBEAT\"}";
                break;
            default:
                cmdName = "UNKNOWN";
                cout << cmdName << " (" << (int)header.message_type << ")" << endl;
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

        if (write(tdL.cl, &respHeader, sizeof(respHeader)) <= 0) {
            perror("[Thread] Error writing response header to client.\n");
            break;
        }

        if (!responseMsg.empty()) {
            if (write(tdL.cl, responseMsg.data(), responseMsg.size()) <= 0) {
                perror("[Thread] Error writing response payload to client.\n");
                break;
            }
        }
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
