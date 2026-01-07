#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <string>
#include <functional>

using namespace std;

using SyslogHandler = function<void(string, string, string, string, string, string)>;

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
    void parse_syslog(const string& raw_message, string& source_ip);
};

#endif