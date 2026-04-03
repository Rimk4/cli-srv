/**
 * @file app2_server/main.cpp
 * @brief Многопоточный сервер AF_UNIX: accept в главном потоке, обработка клиента в std::thread.
 */

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "logger.h"

/** Управляется обработчиками сигналов; читается в цикле accept. */
static volatile sig_atomic_t running = 1;

/**
 * @brief Минимальный обработчик: только устанавливает флаг.
 */
void signalHandler(int)
{
    running = 0;
}

/**
 * @brief Симметричная операция разворота строки.
 * @param text Текст, который нужно развернуть
 * @return Развёрнутая строка
 */
std::string reverse(const std::string& text)
{
    return { text.rbegin(), text.rend() };
}

/**
 * @brief Обрабатывает одно принятое соединение: read → reverse → Logger → write → close.
 *
 * @param [in] clientFd дескриптор после accept; закрывается в этой функции.
 * @param [in] logger    общий журнал; вызовы log сериализуются внутри Logger.
 *
 * @note Выполняется в потоке worker.
 */
void handleClient(int clientFd, Logger& logger)
{
    // Приём запроса одним read (до 1023 байт).
    char buffer[1024];
    
    // Сколько байт пришло от клиента; <=0 — EOF или ошибка, ответа нет.
    ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0)
    {
        close(clientFd);
        return;
    }
    
    std::string original(buffer, bytesRead);
    if (!original.empty() && original.back() == '\n')
        original.pop_back();
    
    // Метка ДО для замера времени reverse (пишется в лог, мкс).
    auto start = std::chrono::high_resolution_clock::now();
    // Полезная нагрузка ответа — байты в обратном порядке.
    std::string reversed = reverse(original);
    // Длительность reverse, мкс.
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start);
    
    logger.log(original, duration.count());
    
    if (write(clientFd, reversed.c_str(), reversed.size()) < 0)
    {
        perror("write");
    }
    
    close(clientFd);
}

/**
 * @brief Инициализация сигналов, сокета, цикл accept, ожидание потоков, очистка узла сокета.
 *
 * @param argc при argc > 1 — путь сокета; при argc > 2 — путь файла журнала.
 */
int main(int argc, char* argv[])
{
    // Путь сокета AF_UNIX на диске (argv[1] или сокет по умолчанию).
    const char* socketPath = "/tmp/ipc.sock";
    // Файл журнала запросов (argv[2] или server.log в cwd).
    const char* logPath = "server.log";
    
    if (argc > 1)
        socketPath = argv[1];
    if (argc > 2)
        logPath = argv[2];
    
    // Игнорировать SIGPIPE: иначе процесс может завершиться при write после закрытия клиентом.
    signal(SIGPIPE, SIG_IGN);
    // Регистрация signalHandler на SIGINT/SIGTERM — сбрасывает running.
    struct sigaction sigAction{};
    sigAction.sa_handler = signalHandler;
    sigemptyset(&sigAction.sa_mask);
    sigAction.sa_flags = 0;
    sigaction(SIGINT, &sigAction, nullptr);
    sigaction(SIGTERM, &sigAction, nullptr);
    
    // Удаляем узел пути
    unlink(socketPath);
    
    // Слушающий потоковый сокет до bind/listen.
    int serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0)
    {
        perror("socket");
        return 1;
    }
    
    // Адрес bind: семейство + sun_path = socketPath.
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    
    if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(serverFd);
        return 1;
    }
    
    if (listen(serverFd, 5) < 0)
    {
        perror("listen");
        close(serverFd);
        return 1;
    }
    
    // Общий лог; запись строки сериализуется внутри Logger.
    Logger logger(logPath);
    // Один std::thread на каждое принятое соединение
    std::vector<std::thread> workers;

    std::cout << "Server started on " << socketPath << std::endl;
    
    while (running)
    {
        // Новое подключение
        int clientFd = accept(serverFd, nullptr, nullptr);
        if (clientFd < 0)
        {
            if (errno == EINTR && !running)
                break;

            if (!running)
                break;

            perror("accept");

            continue;
        }
        
        workers.emplace_back([clientFd, &logger]()
        {
            handleClient(clientFd, logger);
        });
    }
    
    std::cout << "Shutting down, waiting for " << workers.size() 
              << " active connections..." << std::endl;
    
    // Дождаться завершения всех handleClient (ответ отправлен, fd закрыт).
    for (auto& workerThread : workers)
    {
        if (workerThread.joinable())
            workerThread.join();
    }
    
    std::cout << "All connections closed." << std::endl;
    close(serverFd);
    unlink(socketPath);
    
    return 0;
}
