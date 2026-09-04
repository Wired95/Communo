#ifndef _NETWORKHEADERS_H_
#define _NETWORKHEADERS_H_

#ifdef __linux__ 

    #include <sys/socket.h> 
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <string.h>

    #define GET_WIN_CONNEC_ERR_CODE ""

    #define INIT_SOCK
    #define CLEANUP_SOCK
    #define SERVER_SOCKET_OPT SO_REUSEADDR | SO_REUSEPORT

#elif _WIN32

    #include <Ws2tcpip.h>
    #include <Winsock2.h>
    #include <io.h>
    #include <Windows.h>
    #include <iostream>

    #define usleep(X) Sleep(X)

    #undef read
    #define read(X, Y, Z) recv(X, Y, Z, 0)

    #undef close
    #define close(X) closesocket(X)

    #define GET_WIN_CONNEC_ERR_CODE WSAGetLastError()

    #define INIT_SOCK WSADATA wsaData; int wsaErr = WSAStartup(MAKEWORD(2, 2), &wsaData); if (wsaErr != 0) exit(EXIT_FAILURE);
    #define CLEANUP_SOCK WSACleanup()
    #define SERVER_SOCKET_OPT SO_REUSEADDR | SO_REUSEPORT

#endif

#endif // _NETWORKHEADERS_H_
