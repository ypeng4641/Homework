#include <Windows.h>
#include <WinSock2.h>
#include <string>

#ifndef _SOCKET_HELP_H
#define _SOCKET_HELP_H

class Net  
{  
public:  
    static int getPeerToString(SOCKET sock, std::string &ip, unsigned short &port)  
    {  
        struct sockaddr_storage sa;  
        int salen = sizeof(sa);  
        if (::getpeername(sock, (struct sockaddr*)&sa, &salen) == -1) {  
            ip = "?";  
            port = 0;  
            return -1;  
        }  
  
        if (sa.ss_family == AF_INET) {  
            struct sockaddr_in *s = (struct sockaddr_in*)&sa;  
            ip = ::inet_ntoa(s->sin_addr);  
            port = ::ntohs(s->sin_port);  
            return 0;  
        }  
        return -1;  
    }  
  
    static int getLocalToString(SOCKET sock, std::string &ip, unsigned short &port)  
    {  
        struct sockaddr_storage sa;  
        int salen = sizeof(sa);  
  
        if (::getsockname(sock, (struct sockaddr*)&sa, &salen) == -1) {  
            ip = "?";  
            port = 0;  
            return -1;  
        }  
  
        if (sa.ss_family == AF_INET) {  
            struct sockaddr_in *s = (struct sockaddr_in*)&sa;  
            ip = ::inet_ntoa(s->sin_addr);  
            port = ::ntohs(s->sin_port);  
            return 0;  
        }  
        return -1;  
    }  
};//Net

#endif//_SOCKET_HELP_H