#pragma once

#include <memory>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_SERVERS 10
#define DISCOVERY_PORT 8001
#define TASK_REQUESTS_PORT 8002
#define TIMEOUT_SEC 2

#define DISCOVERY_REQUEST "DISCOVERY_REQUEST"
#define DISCOVERY_RESPONSE "DISCOVERY_RESPONSE"

struct Task {
    double start;
    double end;
    double step;
};

inline double TestTask(double x) {
    return x * x;
}

namespace sockets {

struct SocketDeleter {
    void operator()(int* fd) {
        if (fd != nullptr) {
            close(*fd);
            delete fd;
        }
    }
};

using socket_ptr = std::unique_ptr<int, SocketDeleter>;

inline socket_ptr createSocket(int type) {
    socket_ptr sock = {new int(socket(AF_INET, type, 0)), SocketDeleter{}};
    if (*sock < 0) {
        throw std::runtime_error("Socket creation failed");
    }
    return sock;
}

}  // namespace sockets
