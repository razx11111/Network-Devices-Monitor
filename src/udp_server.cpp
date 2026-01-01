#include "udp_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <regex>

UDPSyslogServer::UDPSyslogServer(int port) : port(port), running(false) {
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("UDP socket creation failed");
    }
}

UDPSyslogServer::~UDPSyslogServer() {
    stop();
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

void UDPSyslogServer::set_message_handler(SyslogHandler handler) {
    message_handler = handler;
}

void UDPSyslogServer::start() {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("UDP bind failed");
        return;
    }
    
    std::cout << "[UDP Syslog] Listening on port " << port << std::endl;
    running = true;
    
    char buffer[2048];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (running) {
        int n = recvfrom(socket_fd, buffer, sizeof(buffer) - 1, 0,
                        (struct sockaddr*)&client_addr, &client_len);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string message(buffer);
            std::string source_ip = inet_ntoa(client_addr.sin_addr);
            
            if (message_handler) {
                parse_syslog(message, source_ip);
            }
        }
    }
}

void UDPSyslogServer::stop() {
    running = false;
}

void UDPSyslogServer::parse_syslog(const std::string& raw_message, std::string& source_ip) {
    // Ex: <34>Nov 28 12:00:01 router01 LINK-3-UPDOWN: Interface GigabitEthernet0/1 up
    
    std::regex syslog_rfc5424(R"(^<(\d|\d{2}|1[1-8]\d|19[01])>(\d{1,2})\s(-|([12]\d{3})-(0\d|1[012])-([012]\d|3[01])T([01]\d|2[0-4]):([0-5]\d):([0-5]\d|60)(?:\.(\d{1,6}))?(Z|[+-]\d{2}:\d{2}))\s([\S]{1,255})\s([\S]{1,48})\s([\S]{1,128})\s([\S]{1,32})\s(-|(?:\[.+?(?<!\\)\])+)(?:\s(.+))?$)"); //credite catre un tip de pe regex101.com
    std::regex syslog_rfc3164(R"(<(\d+)>(\S+\s+\d+\s+\d+:\d+:\d+)\s+(\S+)\s+(\S+):\s*(.+))");
    std::smatch matches;

    const char* severity_names[] = {
            "EMERGENCY", "ALERT", "CRITICAL", "ERROR", 
            "WARNING", "NOTICE", "INFO", "DEBUG"
    };

    if (std::regex_search(raw_message, matches, syslog_rfc5424)) {
        // Parse RFC 5424
        int pri = std::stoi(matches[1]);
        std::string severity = severity_names[pri & 0x07];
        
        std::string timestamp = matches[3];
        std::string hostname  = matches[4];
        std::string app_name  = matches[5]; 
        std::string message   = matches[9].matched ? matches[9].str() : "";

        if (message_handler) {
            message_handler(timestamp, hostname, severity, app_name, message);
        }
        return;
    }
    
    if (std::regex_search(raw_message, matches, syslog_rfc3164)) {
        int pri = std::stoi(matches[1]);
        std::string severity = severity_names[pri & 0x07];

        std::string timestamp = matches[2];
        std::string hostname  = matches[3];
        std::string tag       = matches[4]; 
        std::string message   = matches[5];

        if (message_handler) {
            message_handler(timestamp, hostname, severity, tag, message);
        }
        return;
    }
}