// ============================================================
// Phase 3: Multi-threaded LRU Cache Server with TTL
// ============================================================
// What changed from Phase 2:
//   - The plain std::unordered_map<string,string> `store` is
//     replaced by the LRUCache class (lru_cache.h), which adds:
//       - a max capacity, with automatic eviction of the least
//         recently used key when full
//       - optional per-key TTL (expiry)
//   - The command protocol grows slightly:
//       SET <key> <value>              (no expiry)
//       SET <key> <value> EX <seconds> (expires after N seconds)
//       GET <key>
//       DEL <key>
//   - LRUCache does its OWN internal locking (see lru_cache.h),
//     so this file no longer needs its own storeMutex - the
//     thread-safety responsibility has moved into the cache class
//     itself, which is a cleaner design (the cache is safe to use
//     from any thread, on its own, without callers remembering to
//     lock anything).
// ============================================================

#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "lru_cache.h"

// NEW: capacity is now a fixed limit, not "grow forever" like
// Phase 1/2's plain hashmap. 1000 keys chosen as a reasonable
// default for local testing - real Redis-like systems make this
// configurable, but a constant is fine for our purposes.
LRUCache cache(1000);

std::string handleCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd, key, value;
    iss >> cmd >> key;

    if (cmd == "SET") {
        // Read the rest of the line so values can contain spaces.
        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);

        // NEW: check if the value ends with " EX <seconds>" - a
        // simple way to let SET optionally specify a TTL, e.g.:
        //   SET session123 abc123 EX 60
        long ttl = 0;
        size_t exPos = rest.rfind(" EX ");
        if (exPos != std::string::npos) {
            std::string ttlStr = rest.substr(exPos + 4);
            try {
                ttl = std::stol(ttlStr);
                value = rest.substr(0, exPos); // strip " EX <seconds>" off the value
            } catch (...) {
                value = rest; // if parsing fails, treat the whole thing as the value
            }
        } else {
            value = rest;
        }

        cache.set(key, value, ttl);
        return "OK\n";
    }
    else if (cmd == "GET") {
        auto result = cache.get(key);
        if (!result.has_value()) return "(nil)\n";
        return result.value() + "\n";
    }
    else if (cmd == "DEL") {
        bool removed = cache.remove(key);
        return removed ? "OK\n" : "(nil)\n";
    }
    else {
        return "ERROR: unknown command\n";
    }
}

void handleClient(int client_fd) {
    std::cout << "Client connected on thread " << std::this_thread::get_id() << "\n";

    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) break;

        std::string line(buffer);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }

        std::string response = handleCommand(line);
        send(client_fd, response.c_str(), response.size(), 0);
    }

    std::cout << "Client disconnected from thread " << std::this_thread::get_id() << "\n";
    close(client_fd);
}

int main() {
    const int PORT = 6380;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed\n";
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "Listen failed\n";
        return 1;
    }

    std::cout << "LRU KV store (capacity=1000) listening on port " << PORT << "...\n";

    std::vector<std::thread> threads;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            std::cerr << "Accept failed\n";
            continue;
        }

        threads.emplace_back(handleClient, client_fd);
        threads.back().detach();
    }

    close(server_fd);
    return 0;
}