#include "tcp_client_func.h"

static bool read_n_bytes(int socket, void* buffer, int n) {
    int total = 0;
    char* p = static_cast<char*>(buffer);
    while (total < n) {
        int bytes = read(socket, p + total, n - total);
        if (bytes <= 0) return false;
        total += bytes;
    }
    return true;
}

void send_command(int sd, u8 type, string payload) {
    AMPHeader header;
    header.version = 1;
    header.message_type = type;
    header.reserved = 0;
    header.payload_length = htonl(payload.length());  

    if (write(sd, &header, sizeof(AMPHeader)) <= 0) {
        perror("[client] Error writing header");
        return;
    }

    if (payload.length() > 0) {
        if (write(sd, payload.c_str(), payload.length()) <= 0) {
             perror("[client] Error writing payload");
             return;
        }
    }

    AMPHeader respHeader;
    if (!read_n_bytes(sd, &respHeader, sizeof(AMPHeader))) {
        perror("[client] Error reading response header");
        return;
    }

    u32 respLen = ntohl(respHeader.payload_length);
    if (respLen > 4096) {
        fprintf(stderr, "[client] Response too large (%u bytes).\n", respLen);
        return;
    }

    string respPayload(respLen, '\0');
    if (respLen > 0 && !read_n_bytes(sd, &respPayload[0], respLen)) {
        perror("[client] Error reading response payload");
        return;
    }

    cout << "[server response]: " << respPayload << endl;
}
