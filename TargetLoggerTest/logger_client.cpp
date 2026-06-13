// 1. CRITICAL: Define the Windows version BEFORE any socket headers are included
#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600 // Targets Windows Vista and higher (enables inet_pton)
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define closesocket close
#endif

#include "logger_client.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static SOCKET log_socket = INVALID_SOCKET;
static struct sockaddr_in server_addr;

bool connect_logger(const char* host_ip, int port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
#endif

    log_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (log_socket == INVALID_SOCKET) {
        return false;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Fallback approach if compiler complains about inet_pton even with the macro
#ifdef _WIN32
    server_addr.sin_addr.s_addr = inet_addr(host_ip);
    if (server_addr.sin_addr.s_addr == INADDR_NONE) {
        closesocket(log_socket);
        log_socket = INVALID_SOCKET;
        return false;
    }
#else
    if (inet_pton(AF_INET, host_ip, &server_addr.sin_addr) <= 0) {
        closesocket(log_socket);
        log_socket = INVALID_SOCKET;
        return false;
    }
#endif

    return true;
}

void send_to_logger(const char* format, ...) {
    if (log_socket == INVALID_SOCKET) return;

    char log_buffer[1024];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(log_buffer, sizeof(log_buffer), format, args);
    va_end(args);

    if (length > 0) {
        sendto(log_socket, log_buffer, length, 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr));
    }
}

void disconnect_logger(void) {
    if (log_socket != INVALID_SOCKET) {
        closesocket(log_socket);
        log_socket = INVALID_SOCKET;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}
