#ifndef TCP_CLIENT_FUNC_H
#define TCP_CLIENT_FUNC_H

#include "protocol.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>

using namespace std;

void send_command(int sd, u8 type, string payload);

#endif