#ifndef PROTOCOL_H
#define PROTOCOL_H

#define u8 uint8_t  
#define u16 uint16_t 
#define u32 uint32_t 

#include <stdint.h>

struct AMPHeader {
    u8  version;
    u8  message_type;
    u16 reserved;
    u32 payload_length;
    u32 checksum; 
};

#define CMD_AUTH 1
#define CMD_LOG 2
#define CMD_HEARTBEAT 3
#define CMD_SEARCH 4
#define CMD_STATS 5
#define CMD_GET_AGENTS 6  
#define CMD_UPDATE_AGENT 7 
#define CMD_ADD_AGENT 8

#define PORT 9999

#endif