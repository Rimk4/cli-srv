/**
 * @file app1_cli/main.cpp
 * @brief Интерактивный клиент: строковый запрос по AF_UNIX `SOCK_STREAM`, ответ сервера в stdout.
 */

#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>

/**
 * @brief RAII-обёртка над дескриптором сокета (fcntl-совместимый fd).
 */
class SocketGuard {
    int fd;
public:
    /** @param f дескриптор для владения; при `f >= 0` будет закрыт в деструкторе. */
    explicit SocketGuard(int f = -1) : fd(f) {}
    /** @brief Вызывает `close(2)` для захваченного fd, если он не отрицательный. */
    ~SocketGuard() { if (fd >= 0) close(fd); }
};

/**
 * @brief Одна транзакция «отправить полезную нагрузку — получить ответ».
 *
 * @param socket_path путь узла `AF_UNIX` (существующий сокет сервера).
 * @param input       данные для `write(2)`; размер произвольный, без завершающего `\0` на проводе.
 * @param[out] out_response ответ, прочитанный одним `read` до 1023 байт, трактуется как C-строка.
 *
 * @return `true` при успешном обмене, иначе `false` (в stderr — краткая диагностика).
 */
bool send_and_receive(const char* socket_path, const std::string& input, std::string& out_response) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;
    SocketGuard guard(sock);

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error: Server not running\n";
        return false;
    }

    if (write(sock, input.c_str(), input.size()) != (ssize_t)input.size()) {
        std::cerr << "Error: Failed to send\n";
        return false;
    }

    char buffer[1024];
    ssize_t n = read(sock, buffer, sizeof(buffer) - 1);
    if (n < 0) {
        std::cerr << "Error: Failed to read\n";
        return false;
    }
    buffer[n] = '\0';
    out_response = buffer;
    return true;
}

/**
 * @brief Точка входа: цикл `getline`, для каждой непустой строки — запрос к серверу.
 *
 * @param argc количество аргументов; при `argc > 1` `argv[1]` — путь сокета, иначе `/tmp/ipc.sock`.
 */
int main(int argc, char* argv[]) {
    const char* socket_path = (argc > 1) ? argv[1] : "/tmp/ipc.sock";
    signal(SIGPIPE, SIG_IGN);

    std::string input;
    while (std::cout << "Enter string: " << std::flush, std::getline(std::cin, input)) {
        if (input.empty()) continue;
        
        std::string response;
        if (!send_and_receive(socket_path, input, response)) return 1;
        std::cout << "Reversed: " << response << '\n';
    }
    return 0;
}
