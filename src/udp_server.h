#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <string>
#include <functional>

class UDPSyslogServer {
private:
    int socket_fd;
    int port;
    bool running;
    
    // Callback pentru procesare mesaj
    std::function<void(const std::string&, const std::string&)> message_handler;
    
public:
    UDPSyslogServer(int port);
    ~UDPSyslogServer();
    
    void set_message_handler(std::function<void(const std::string&, const std::string&)> handler);
    void start();
    void stop();
    
private:
    void parse_syslog(const std::string& raw_message, std::string& source_ip);
};

#endif