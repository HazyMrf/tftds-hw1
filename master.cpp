#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <unordered_set>
#include <future>
#include <queue>

#include "common.h"

struct Server {

    sockaddr_in addr;
    bool operator==(const Server& other) const {
        return addr.sin_addr.s_addr == other.addr.sin_addr.s_addr;
    }
};

template <>
struct std::hash<Server> {
    std::size_t operator()(const Server& s) const noexcept {
        return s.addr.sin_addr.s_addr;
    }
};

class Master {
    std::unordered_set<Server> servers;

    // Good impl would have this function running in the background thread, but this leads to mutexes.
    // So in ideal case we would use fiber/coro pool instead of all this, but its too complex for this task
    void discover_servers() {
        auto sock = sockets::createSocket(SOCK_DGRAM);
    
        int broadcast = 1;
        setsockopt(*sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
        
        timeval timeout{TIMEOUT_SEC, 0};
        setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sockaddr_in broadcast_addr{};
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_port = htons(DISCOVERY_PORT);
        broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

        if (sendto(*sock, DISCOVERY_REQUEST, sizeof(DISCOVERY_REQUEST), 0, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            throw std::runtime_error("Broadcast failed");
        }

        char buffer[256];
        sockaddr_in server_addr;
        socklen_t addr_len = sizeof(server_addr);

        while (true) {
            if (recvfrom(*sock, buffer, sizeof(buffer)-1, 0, (sockaddr*)&server_addr, &addr_len) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                continue;
            }

            server_addr.sin_port = htons(DISCOVERY_PORT);
            servers.emplace(server_addr);
            std::cout << "New server: " << inet_ntoa(server_addr.sin_addr) << std::endl;
        }
    }

    double process_task(const Server& server, const Task& task) {
        auto sock = sockets::createSocket(SOCK_STREAM);

        timeval timeout{TIMEOUT_SEC, 0};
        setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sockaddr_in addr = server.addr;
        addr.sin_port = htons(TASK_REQUESTS_PORT);

        if (connect(*sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("Connection failed");
        }

        if (send(*sock, &task, sizeof(task), 0) < 0) {
            throw std::runtime_error("Send failed");
        }

        double result;
        if (recv(*sock, &result, sizeof(result), 0) < 0) {
            throw std::runtime_error("Receive failed");
        }

        return result;
    }

    double distribute_tasks(const std::vector<Task>& tasks) {
        double total = 0.0;
        size_t current_task = 0;
        
        while (current_task < tasks.size()) {
            if (servers.empty()) {
                discover_servers();
                std::cout << "Found " << servers.size() << " servers for execution" << std::endl;
                continue;
            }

            std::vector<std::future<double>> batch;

            std::mutex mtx;
            std::unordered_set<Server> failed_servers;

            // a.k.a multithreading via std::async.
            for (const auto& server: servers) {
                if (current_task >= tasks.size()) break;

                batch.push_back(
                    std::async(std::launch::async,
                        [this, &mtx, &failed_servers, server, task = tasks[current_task]]() {
                            try {
                                return process_task(server, task);
                            } catch (...) {
                                std::lock_guard guard(mtx);
                                failed_servers.insert(server);
                                return 0.0;
                            }
                        }
                    )
                );
                current_task++;
            }

            for (auto& future : batch) {
                total += future.get();
            }

            std::lock_guard guard(mtx); // just in case
            for (const auto& failed_server: failed_servers) {
                servers.erase(failed_server);
            }
        }
        
        return total;
    }

public:
    double run(double start, double end, double step) {
        std::vector<Task> tasks;

        // Here we should split the work among threads according to their number
        // But to imitate more work we use constant. This would be applicable with 
        // fiber/coro pool btw

        static constexpr double kIntegralStep = 10;
        for (double x = start; x < end; x += kIntegralStep) {
            tasks.push_back({x, std::min(x + kIntegralStep, end), step});
        }

        return distribute_tasks(tasks);
    }
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Expected start, end & step" << std::endl;
        return 1;
    }

    try {
        Master master;
        double result = master.run(std::stod(argv[1]), std::stod(argv[2]), std::stod(argv[3]));
        std::cout << "Result: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}