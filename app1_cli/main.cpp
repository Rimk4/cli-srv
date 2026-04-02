/**
 * @file app1_cli/main.cpp
 * @brief Интерактивный клиент: строковый запрос по AF_UNIX SOCK_STREAM, ответ сервера в stdout.
 */

#include <cstring>
#include <iostream>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/**
 * @brief RAII-обёртка над дескриптором сокета.
 */
class SocketGuard
{
    /**
    * @brief Дескриптор сокета, который закрывается в деструкторе; \c -1 — нет активного сокета.
    */
    int fd;
public:
    /**
    * @param ownedFd дескриптор для владения; при ownedFd >= 0 будет закрыт в деструкторе.
    */
    explicit SocketGuard(int ownedFd = -1) : fd(ownedFd) {}

    /**
    * @brief Вызывает close для захваченного fd, если он не отрицательный.
    */
    ~SocketGuard() { if (fd >= 0) close(fd); }
};

/**
 * @brief Проверка доступности сервера
 *
 * @return true если сервер доступен, иначе false.
 */
 bool probeServer(const char* socketPath)
 {
     // Проверяем, существует ли файл сокета
     if (access(socketPath, F_OK) != 0) {
         std::cerr << "Error: Server socket not found at " << socketPath << "\n";
         return false;
     }
     return true;
 }

/**
 * @brief Одна транзакция: отправить полезную нагрузку — получить ответ.
 *
 * @param [in] socketPath путь к существующему сокету сервера.
 * @param [in] input       данные для write; размер произвольный, без завершающего \0.
 * @param [out] outResponse ответ, прочитанный одним read до 1023 байт, C-строка.
 *
 * @return true при успешном обмене, иначе false (в stderr — краткая диагностика).
 */
bool sendAndReceive(const char* socketPath, const std::string& input, std::string& outResponse)
{
    // Клиентский потоковый сокет (еще не привязан к пути сервера).
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock < 0)
        return false;

    // Гарантирует close(sock) при выходе из функции (в том числе по return).
    SocketGuard guard(sock);

    // Адрес семейства AF_UNIX: sun_path должен совпадать с сокетом сервера.
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);

    // Нет слушающего сокета по указанному пути или отказ в подключении.
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "Error: Server not running\n";
        return false;
    }

    // Запрос должен уйти целиком.
    if (write(sock, input.c_str(), input.size()) != (ssize_t)input.size())
    {
        std::cerr << "Error: Failed to send\n";
        return false;
    }

    // Сырые байты ответа; в конеце нужно дописать '\0'.
    char buffer[1024];
    ssize_t numRead = read(sock, buffer, sizeof(buffer) - 1);
    // Ошибка read.
    if (numRead < 0)
    {
        std::cerr << "Error: Failed to read\n";
        return false;
    }

    buffer[numRead] = '\0';
    outResponse.assign(buffer, static_cast<size_t>(numRead));

    return true;
}

/**
 * @brief Точка входа: цикл getline, для каждой непустой строки — запрос к серверу.
 *
 * @param argc количество аргументов; при argc > 1 argv[1] — путь сокета, иначе сокет по умолчанию.
 */
int main(int argc, char* argv[])
{
    // Узел сокета сервера: явный путь или сокет по умолчанию.
    const char* socketPath = (argc > 1) ? argv[1] : "/tmp/ipc.sock";
    signal(SIGPIPE, SIG_IGN);

    if (!probeServer(socketPath))
        return 1;

    // Очередная строка из stdin.
    std::string input;
    while (std::cout << "Enter string: " << std::flush, std::getline(std::cin, input))
    {
        if (input.empty())
            continue;
        
        // Тело ответа сервера после одной транзакции send/recv.
        std::string response;
        if (!sendAndReceive(socketPath, input, response))
            return 1;

        std::cout << "Reversed: " << response << '\n';
    }

    return 0;
}
