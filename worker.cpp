#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>

#include "common.h"

class Worker {
private:
    const int discovery_port;
    const int task_requests_port;

    double computeIntegral(const Task& task) {
        double result = 0.0;

        for (double x = task.start; x < task.end; x += task.step) {
            result += TestTask(x) * task.step;
        }

        return result;
    }

    auto createSocket(int type, int port) {
        auto sock = sockets::createSocket(type);

        int reuse = 1;
        setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(*sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw std::runtime_error("Bind failed");
        }

        return sock;
    }

    void handleDiscovery() {
        auto sock = createSocket(SOCK_DGRAM, discovery_port);
        char buffer[256];

        while (true) {
            sockaddr_in master_addr;
            socklen_t addr_len = sizeof(master_addr);
            
            ssize_t n = recvfrom(*sock, buffer, sizeof(buffer)-1, 0,
                                reinterpret_cast<sockaddr*>(&master_addr), &addr_len);
            
            if (n > 0 && strcmp(buffer, DISCOVERY_REQUEST) == 0) {
                sendto(*sock, DISCOVERY_RESPONSE, sizeof(DISCOVERY_RESPONSE), 0,
                      reinterpret_cast<sockaddr*>(&master_addr), addr_len);
            }
        }
    }

    void handleTasks() {
        auto sock = createSocket(SOCK_STREAM, task_requests_port);
        listen(*sock, 5);

        while (true) {
            sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            int client_sock = accept(*sock, 
                                   reinterpret_cast<sockaddr*>(&client_addr), 
                                   &addr_len);
            
            if (client_sock < 0) continue;

            Task task;
            if (recv(client_sock, &task, sizeof(task), 0) == sizeof(Task)) {
                double result = computeIntegral(task);
                send(client_sock, &result, sizeof(result), 0);
            }
            
            close(client_sock);
        }
    }

public:
    Worker(int dp, int tp) : discovery_port(dp), task_requests_port(tp) {}

    void run() {
        std::thread{&Worker::handleDiscovery, this}.detach();
        handleTasks();
    }
};

int main() {
    try {
        Worker worker(DISCOVERY_PORT, TASK_REQUESTS_PORT);
        worker.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}