#ifndef TCP_SERVER_FUNC_H
#define TCP_SERVER_FUNC_H

#include <string>
#include "protocol.h"

using namespace std;

struct thData {
    int idThread; // thread id
    int cl;       // client descriptor
};

// Expose this so main.cpp (UDP) can call it
void broadcast_to_dashboards(string jsonLog);

void *treat(void *arg);
void raspunde(void *arg);
bool read_n_bytes(int socket, void* buffer, int n);

#endif