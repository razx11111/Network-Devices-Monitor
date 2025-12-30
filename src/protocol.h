#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

#define CMD_AUTH 1
#define CMD_LOG  2
#define CMD_HEARTBEAT 3
#define PORT 9999

#define OK 0
#define ERR 101

#define u8 uint8_t  
#define u16 uint16_t 
#define u32 uint32_t 

struct AMPHeader {
    u8 version;
    u8 message_type;
    u16 reserved;
    u32 payload_length;
};

static_assert(sizeof(AMPHeader) == 8, "AMPHeader must be 8 bytes");

typedef struct thData {
    int idThread;
    int cl;
}thData;

#endif
