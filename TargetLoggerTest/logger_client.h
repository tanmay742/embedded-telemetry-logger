#ifndef LOGGER_CLIENT_H
#define LOGGER_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

bool connect_logger(const char* host_ip, int port);
void send_to_logger(const char* format, ...);
void disconnect_logger(void);

#ifdef __cplusplus
}
#endif

#endif // LOGGER_CLIENT_H

