// ============================================================
// Phase 2: Multi-threaded In-Memory Key-Value Store
// ============================================================
// What changed from Phase 1:
//   - The main loop no longer handles a client directly.
//     Instead, for every accepted connection, it spawns a NEW
//     thread to handle that client, then immediately goes back
//     to accept()-ing the next one. This lets multiple clients
//     be served AT THE SAME TIME.
//   - Since multiple threads now read/write the shared `store`
//     map concurrently, every access to `store` is protected by
//     a std::mutex to prevent race conditions / data corruption.
// ============================================================

#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <thread>          // NEW: std::thread
#include <mutex>           // NEW: std::mutex, std::lock_guard
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

std::unordered_map<std::string, std::string> store;

// NEW: this mutex protects `store`. ANY code that reads or
// writes `store` must hold this lock first. Without it, two
// threads modifying the hashmap at the same time can corrupt
// its internal structure (not just "wrong value" -- potential
// crash, since unordered_map isn't thread-safe by default).
std::mutex storeMutex;

std::string handleCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd, key, value;
    iss >> cmd >> key;

    if (cmd == "SET") {
        std::getline(iss, value);
        if (!value.empty() && value[0] == ' ') value.erase(0, 1);

        // NEW: lock before touching the shared map.
        // lock_guard automatically unlocks when it goes out of
        // scope (end of this if-block) -- even if an exception
        // were thrown, so we can't accidentally forget to unlock.
        std::lock_guard<std::mutex> lock(storeMutex);
        store[key] = value;
        return "OK\n";
    }
    else if (cmd == "GET") {
        std::lock_guard<std::mutex> lock(storeMutex);
        auto it = store.find(key);
        if (it == store.end()) return "(nil)\n";
        return it->second + "\n";
    }
    else if (cmd == "DEL") {
        std::lock_guard<std::mutex> lock(storeMutex);
        size_t erased = store.erase(key);
        return erased ? "OK\n" : "(nil)\n";
    }
    else {
        return "ERROR: unknown command\n";
    }
}

// NEW: this function is what each thread runs. It's basically
// the exact same client-handling loop from Phase 1's main(),
// just moved into its own function so std::thread can call it.
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

    if (listen(server_fd, 10) < 0) {  // increased backlog since we now expect more concurrent connections
        std::cerr << "Listen failed\n";
        return 1;
    }

    std::cout << "Multi-threaded KV store listening on port " << PORT << "...\n";

    // NEW: keep track of every thread we spawn, so we could join
    // them on shutdown if we wanted a clean exit (not critical
    // for this project, but good practice to know about).
    std::vector<std::thread> threads;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            std::cerr << "Accept failed\n";
            continue;
        }

        // NEW: this is the core change. Instead of calling
        // handleClient(client_fd) directly here (which would
        // block this loop until that client disconnects), we
        // spawn a NEW thread to run it, and immediately loop
        // back to accept() the next client.
        threads.emplace_back(handleClient, client_fd);

        // NEW: detach lets this thread run independently -- we
        // don't need to manually join() it later, since we don't
        // know in advance when each client will disconnect.
        threads.back().detach();
    }

    close(server_fd);
    return 0;
}
