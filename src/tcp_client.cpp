#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "tcp_client_func.h"

using namespace std;

int main(int  argc, char *argv[]) {
    int sd;
    struct sockaddr_in server;
    if (argc < 2) {
        cerr << "[client] Usage: " << argv[0] << " <server_ip>\n";
        return 1;
    }
    string server_ip = argv[1];

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[client] Error at socket().\n");
        return errno;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, server_ip.c_str(), &server.sin_addr) != 1) {
        perror("[client] Invalid server IP.\n");
        return 1;
    }

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
        perror("[client] Error at connect().\n");
        return errno;
    }

    printf("[client] Connected to %s:%d\n", server_ip.data(), PORT);

    while (1) {
        cout << "Enter command (auth | log | hb | exit): ";
        
        string opt;
        cin >> opt;

        string payload = "";

        if (opt == "auth") {
            payload = "{\"user\":\"agent01\", \"key\":\"secret123\"}";
            send_command(sd, CMD_AUTH, payload);
        }
        else if (opt == "log") {
            payload = "{\"timestamp\":\"2025-11-28 22:13:00\",\"hostname\":\"agent01\",\"severity\":\"CRITICAL\",\"application\":\"SSH\",\"message\":\"Failed password for user root\", \"PID\":\"1234\"}";
            send_command(sd, CMD_LOG, payload);
        }
        else if (opt == "hb") {
            send_command(sd, CMD_HEARTBEAT, "");
        }
        else if (opt == "exit") {
            break;
        } else {
            cout << "NOT a command\n";
        }
    }

    close(sd);
    return 0;
}
