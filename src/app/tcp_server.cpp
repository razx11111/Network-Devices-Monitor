#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <algorithm> // Added for string manipulation if needed

#include "protocol.h"
#include "udp_server.h"
#include "tcp_server_func.h"
#include "SQLite_manager.h"


SQLiteManager* g_db_manager = nullptr;
UDPSyslogServer* g_udp_server = nullptr;
extern int errno;

using namespace std;

// Helper to escape JSON strings locally for UDP
string local_json_escape(const string& str) {
    string output;
    for (char c : str) {
        if (c == '"') output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\n') output += "\\n";
        else output += c;
    }
    return output;
}

int main() {
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sd;
    int i = 0;

    g_db_manager = new SQLiteManager("network_monitor.db");
    if (!g_db_manager->init_database()) {
        cerr << "Failed to initialize database!" << endl;
        return 1;
    }
    cout << "[server] Database initialized successfully." << endl;
    
    g_udp_server = new UDPSyslogServer(514); 
    
    g_udp_server->set_message_handler([](string ts, string host, string sev, string app, string msg) {
        if (g_db_manager) {
            sleep(1);
            // 'app' might be "sshd[1234]"
            string app_name = app;
            string pid = "0"; // Default PID
            size_t pid_start = app.find('[');
            if (pid_start != string::npos) {
                size_t pid_end = app.find(']', pid_start);
                if (pid_end != string::npos) {
                    app_name = app.substr(0, pid_start);
                    pid = app.substr(pid_start + 1, pid_end - pid_start - 1);
                }
            }
            
            // Insert into DB
            g_db_manager->insert_log(ts, host, sev, app_name, msg, pid, "syslog");
            cout << "[UDP] Log saved from " << host << endl;

            // FIX 2: Broadcast to Dashboards
            // We construct the JSON manually here
            string jsonLog = "{";
            jsonLog += "\"timestamp\":\"" + local_json_escape(ts) + "\",";
            jsonLog += "\"hostname\":\"" + local_json_escape(host) + "\",";
            jsonLog += "\"pid\":\"" + local_json_escape(pid) + "\",";
            jsonLog += "\"severity\":\"" + local_json_escape(sev) + "\",";
            jsonLog += "\"application\":\"" + local_json_escape(app_name) + "\",";
            jsonLog += "\"message\":\"" + local_json_escape(msg) + "\",";
            jsonLog += "\"source\":\"UDP\""; 
            jsonLog += "}";

            broadcast_to_dashboards(jsonLog);
        }
    }); 

    thread udp_thread([&]() {
        g_udp_server->start();
    });
    udp_thread.detach();

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[server] Error at socket().\n");
        return errno;
    }

    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
        perror("[server] Error at bind().\n");
        return errno;
    }

    if (listen(sd, 2) == -1) {
        perror("[server] Error at listen().\n");
        return errno;
    }

    while (1) {
        int client;
        pthread_t tid;
        thData *td;
        socklen_t length = sizeof(from);

        printf("[server] Listening on port %d...\n", PORT);
        fflush(stdout);

        if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0) {
            perror("[server] Error at accept().\n");
            continue;
        }

        td = (struct thData *)malloc(sizeof(struct thData));
        td->idThread = i++;
        td->cl = client;

        pthread_create(&tid, NULL, &treat, td);
    }
    delete g_db_manager;
    return 0;
}