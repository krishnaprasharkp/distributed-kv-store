// ============================================================
// Phase 1: Single-threaded In-Memory Key-Value Store
// ============================================================
// What this does:
//   - Listens on a TCP port
//   - Accepts ONE client connection at a time (no threading yet)
//   - Understands 3 commands sent as plain text:
//       SET <key> <value>
//       GET <key>
//       DEL <key>
//   - Stores everything in an in-memory std::unordered_map
//
// Why single-threaded first:
//   We want to prove the core logic (parsing commands, storing
//   data, responding over a socket) works correctly BEFORE we
//   add the complexity of multiple threads. Debugging threading
//   bugs on top of broken core logic is much harder.
// ============================================================

#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <cstring>
#include <unistd.h>       // close()
#include <arpa/inet.h>    // sockaddr_in, htons, etc.
#include <sys/socket.h>   // socket(), bind(), listen(), accept()

// The actual data store. This is just a hash map, same as
// std::unordered_map<std::string, std::string> anywhere else —
// nothing magic. This IS the "database."
std::unordered_map<std::string, std::string> store;

// Parses one line of input like "SET name Krishna" and executes it.
// Returns the text response to send back to the client.
std::string handleCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd, key, value;
    iss >> cmd >> key;

    if (cmd == "SET") {
        // Everything after the key (could contain spaces) is the value.
        std::getline(iss, value);
        if (!value.empty() && value[0] == ' ') value.erase(0, 1);
        store[key] = value;
        return "OK\n";
    }
    else if (cmd == "GET") {
        auto it = store.find(key);
        if (it == store.end()) return "(nil)\n";
        return it->second + "\n";
    }
    else if (cmd == "DEL") {
        size_t erased = store.erase(key);
        return erased ? "OK\n" : "(nil)\n";
    }
    else {
        return "ERROR: unknown command\n";
    }
}

int main() {
    const int PORT = 6380; // deliberately not 6379 (Redis's port), to avoid confusion

    // 1. Create a TCP socket.
    //    AF_INET = IPv4, SOCK_STREAM = TCP (reliable, ordered byte stream)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    // Allow quick restart of the server without "Address already in use" errors
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind the socket to a specific port on this machine.
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // listen on all network interfaces
    address.sin_port = htons(PORT);       // htons = convert to network byte order

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed\n";
        return 1;
    }

    // 3. Start listening for incoming connections. Backlog of 5 = up to 5
    //    pending connections can queue before we accept() them.
    if (listen(server_fd, 5) < 0) {
        std::cerr << "Listen failed\n";
        return 1;
    }

    std::cout << "KV store listening on port " << PORT << "...\n";

    // 4. Main loop: accept ONE client, serve it until it disconnects,
    //    then go back to accepting the next one. This is the
    //    "single-threaded" limitation we'll remove in Phase 2.
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            std::cerr << "Accept failed\n";
            continue;
        }

        std::cout << "Client connected.\n";

        char buffer[1024];
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytesRead = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytesRead <= 0) {
                // 0 = client closed connection, <0 = error
                break;
            }

            std::string line(buffer);
            // Strip trailing newline/carriage return, if present
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }

            std::string response = handleCommand(line);
            send(client_fd, response.c_str(), response.size(), 0);
        }

        std::cout << "Client disconnected.\n";
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
