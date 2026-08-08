

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using Clock = std::chrono::steady_clock;

int connectToServer() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(6380);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}


double sendAndTime(int sock, const std::string& command) {
    char buffer[1024];
    std::string msg = command + "\n";

    auto start = Clock::now();
    send(sock, msg.c_str(), msg.size(), 0);
    ssize_t n = read(sock, buffer, sizeof(buffer) - 1);
    (void)n; // response content isn't needed here, just the round-trip time
    auto end = Clock::now();

    return std::chrono::duration<double, std::micro>(end - start).count();
}


void worker(int threadId, int requestsPerThread, std::vector<double>& latencies, std::atomic<int>& successCount) {
    int sock = connectToServer();
    if (sock < 0) {
        std::cerr << "Thread " << threadId << " failed to connect\n";
        return;
    }

    for (int i = 0; i < requestsPerThread; i++) {
        std::string key = "bench_key_" + std::to_string(threadId) + "_" + std::to_string(i % 100);
      
        double latency;
        if (i % 2 == 0) {
            latency = sendAndTime(sock, "SET " + key + " value" + std::to_string(i));
        } else {
            latency = sendAndTime(sock, "GET " + key);
        }
        latencies[i] = latency;
        successCount++;
    }

    close(sock);
}


double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
    return sorted[idx];
}

int main(int argc, char* argv[]) {
    int numThreads = 4;
    int requestsPerThread = 1000;

    if (argc >= 2) numThreads = std::stoi(argv[1]);
    if (argc >= 3) requestsPerThread = std::stoi(argv[2]);

    std::cout << "Benchmarking with " << numThreads << " threads, "
              << requestsPerThread << " requests/thread "
              << "(" << (numThreads * requestsPerThread) << " total ops)...\n";

   
    std::vector<std::vector<double>> perThreadLatencies(numThreads, std::vector<double>(requestsPerThread));
    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;

    auto overallStart = Clock::now();

    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back(worker, t, requestsPerThread, std::ref(perThreadLatencies[t]), std::ref(successCount));
    }
    for (auto& t : threads) {
        t.join();
    }

    auto overallEnd = Clock::now();
    double totalSeconds = std::chrono::duration<double>(overallEnd - overallStart).count();

    std::vector<double> allLatencies;
    for (auto& threadLatencies : perThreadLatencies) {
        allLatencies.insert(allLatencies.end(), threadLatencies.begin(), threadLatencies.end());
    }
    std::sort(allLatencies.begin(), allLatencies.end());

    double totalOps = successCount.load();
    double throughput = totalOps / totalSeconds;
    double avgLatency = 0;
    for (double l : allLatencies) avgLatency += l;
    avgLatency /= allLatencies.empty() ? 1 : allLatencies.size();

    std::cout << "\n--- Results ---\n";
    std::cout << "Total time:       " << totalSeconds << " sec\n";
    std::cout << "Successful ops:   " << (long)totalOps << "\n";
    std::cout << "Throughput:       " << (long)throughput << " ops/sec\n";
    std::cout << "Avg latency:      " << avgLatency << " us\n";
    std::cout << "p50 latency:      " << percentile(allLatencies, 0.50) << " us\n";
    std::cout << "p95 latency:      " << percentile(allLatencies, 0.95) << " us\n";
    std::cout << "p99 latency:      " << percentile(allLatencies, 0.99) << " us\n";

    return 0;
}
