#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    const int PORT = 6380;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed. Is the server running?\n";
        return 1;
    }

    std::cout << "Connected to KV store. Type commands (SET/GET/DEL), or 'exit' to quit.\n";

    std::string line;
    char buffer[1024];
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;
        if (line.empty()) continue;

        line += "\n";
        send(sock, line.c_str(), line.size(), 0);

        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = read(sock, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            std::cout << "Server closed connection.\n";
            break;
        }
        std::cout << buffer;
    }

    close(sock);
    return 0;
}
