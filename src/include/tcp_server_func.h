#ifndef TCP_SERVER_FUNC_H
#define TCP_SERVER_FUNC_H

#include <string>
#include "protocol.h"

// Expose this so main.cpp (UDP) can call it
void broadcast_to_dashboards(std::string jsonLog);

void *treat(void *arg);
void raspunde(void *arg);
bool read_n_bytes(int socket, void* buffer, int n);

#endif