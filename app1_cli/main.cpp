#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>

class SocketGuard {
    int fd;
public:
    explicit SocketGuard(int f = -1) : fd(f) {}
    ~SocketGuard() { if (fd >= 0) close(fd); }
};

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
