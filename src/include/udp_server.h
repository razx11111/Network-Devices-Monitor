#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <string>
#include <functional>

using SyslogHandler = std::function<void(std::string, std::string, std::string, std::string, std::string)>;

class UDPSyslogServer {
private:
    int socket_fd;
    int port;
    bool running;
    SyslogHandler message_handler;
       
public:
    UDPSyslogServer(int port);
    ~UDPSyslogServer();
    
    void set_message_handler(SyslogHandler handler);
    void start();
    void stop();
    
private:
    void parse_syslog(const std::string& raw_message, std::string& source_ip);
};

#endif