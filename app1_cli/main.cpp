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
 * @brief RAII-обёртка над дескриптором сокета (fcntl-совместимый fd).
 */
class SocketGuard
{
    int fd;
public:
    /**
    * @param f дескриптор для владения; при f >= 0 будет закрыт в деструкторе.
    */
    explicit SocketGuard(int f = -1) : fd(f) {}

    /**
    * @brief Вызывает close для захваченного fd, если он не отрицательный.
    */
    ~SocketGuard() { if (fd >= 0) close(fd); }
};

/**
 * @brief Одна транзакция: отправить полезную нагрузку — получить ответ.
 *
 * @param [in] socket_path путь узла AF_UNIX (существующий сокет сервера).
 * @param [in] input       данные для write; размер произвольный, без завершающего \0.
 * @param [out] out_response ответ, прочитанный одним read до 1023 байт, C-строка.
 *
 * @return true при успешном обмене, иначе false (в stderr — краткая диагностика).
 */
bool send_and_receive(const char* socket_path, const std::string& input, std::string& out_response)
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
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

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
    ssize_t n = read(sock, buffer, sizeof(buffer) - 1);  // Прочитано n байт
    // Ошибка read.
    if (n < 0)
    {
        std::cerr << "Error: Failed to read\n";
        return false;
    }

    buffer[n] = '\0';
    out_response = buffer;

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
    const char* socket_path = (argc > 1) ? argv[1] : "/tmp/ipc.sock";
    signal(SIGPIPE, SIG_IGN);

    // Очередная строка из stdin.
    std::string input;
    while (std::cout << "Enter string: " << std::flush, std::getline(std::cin, input))
    {
        if (input.empty())
            continue;
        
        // Тело ответа сервера после одной транзакции send/recv.
        std::string response;
        if (!send_and_receive(socket_path, input, response))
            return 1;

        std::cout << "Reversed: " << response << '\n';
    }

    return 0;
}
