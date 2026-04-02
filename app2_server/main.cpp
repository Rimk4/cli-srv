/**
 * @file app2_server/main.cpp
 * @brief Многопоточный сервер AF_UNIX: accept в главном потоке, обработка клиента в `std::thread`.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "logger.h"

/** Управляется обработчиками сигналов; читается в цикле accept. */
static volatile sig_atomic_t running = 1;

/** Минимальный обработчик: только устанавливает флаг. */
void signal_handler(int) {
    running = 0;
}

/** @brief Симметричная операция разворота строки. */
std::string reverse(const std::string& str) {
    return {str.rbegin(), str.rend()};
}

/**
 * @brief Обрабатывает одно принятое соединение: read → reverse → Logger → write → close.
 *
 * @param client_fd дескриптор после `accept(2)`; всегда закрывается в этой функции (включая ранние выходы).
 * @param logger    общий журнал; вызовы `log` сериализуются внутри Logger.
 *
 * @note Выполняется в потоке worker.
 */
void handle_client(int client_fd, Logger& logger) {
    char buffer[1024];
    
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    
    std::string original(buffer, bytes_read);
    if (!original.empty() && original.back() == '\n')
        original.pop_back();
    
    auto start = std::chrono::high_resolution_clock::now();
    std::string reversed = reverse(original);
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start);
    
    logger.log(original, duration.count());
    
    if (write(client_fd, reversed.c_str(), reversed.size()) < 0) {
        perror("write");
    }
    
    close(client_fd);
}

/**
 * @brief Инициализация сигналов, сокета, цикл accept, ожидание потоков, очистка узла сокета.
 *
 * @param argc при `argc > 1` — путь сокета; при `argc > 2` — путь файла журнала.
 */
int main(int argc, char* argv[]) {
    const char* SOCKET_PATH = "/tmp/ipc.sock";
    const char* LOG_PATH = "server.log";
    
    if (argc > 1) SOCKET_PATH = argv[1];
    if (argc > 2) LOG_PATH = argv[2];
    
    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    
    unlink(SOCKET_PATH);
    
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    
    Logger logger(LOG_PATH);
    std::vector<std::thread> workers;
    workers.reserve(8);
    
    std::cout << "Server started on " << SOCKET_PATH << std::endl;
    
    while (running) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR && !running) break;
            if (!running) break;
            perror("accept");
            continue;
        }
        
        workers.emplace_back([client_fd, &logger]() {
            handle_client(client_fd, logger);
        });
    }
    
    std::cout << "Shutting down, waiting for " << workers.size() 
              << " active connections..." << std::endl;
    
    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }
    
    std::cout << "All connections closed." << std::endl;
    close(server_fd);
    unlink(SOCKET_PATH);
    
    return 0;
}
