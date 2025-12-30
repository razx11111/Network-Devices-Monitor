#ifndef TCP_SERVER_FUNC_H
#define TCP_SERVER_FUNC_H

using namespace std;

void *treat(void *arg);
void raspunde(void *arg);
bool read_n_bytes(int socket, void* buffer, int n);

#endif
