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

void UDPSyslogServer::set_message_handler(
    std::function<void(const std::string&, const std::string&)> handler) {
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
    // Parser simplu pentru RFC 3164: <PRI>TIMESTAMP HOSTNAME TAG: MESSAGE
    // Ex: <34>Nov 28 12:00:01 router01 LINK-3-UPDOWN: Interface GigabitEthernet0/1 up
    
    std::regex syslog_regex(R"(<(\d+)>(\S+\s+\d+\s+\d+:\d+:\d+)\s+(\S+)\s+(\S+):\s*(.+))");
    std::smatch matches;
    
    if (std::regex_search(raw_message, matches, syslog_regex)) {
        int priority = std::stoi(matches[1]);
        int severity = priority & 0x07;  // Last 3 bits
        int facility = priority >> 3;     // First 5 bits
        
        std::string timestamp = matches[2];
        std::string hostname = matches[3];
        std::string tag = matches[4];
        std::string message = matches[5];
        
        // Map severity number to string
        const char* severity_names[] = {
            "EMERGENCY", "ALERT", "CRITICAL", "ERROR", 
            "WARNING", "NOTICE", "INFO", "DEBUG"
        };
        std::string severity_str = severity_names[severity];
        
        std::cout << "[UDP] " << hostname << " [" << severity_str << "] " << message << std::endl;
        
        // Call handler cu datele parsate
        if (message_handler) {
            message_handler(raw_message, source_ip);
        }
        
        // Aici salvezi în database (vezi mai jos)
    } else {
        std::cout << "[UDP] Could not parse syslog: " << raw_message << std::endl;
    }
}