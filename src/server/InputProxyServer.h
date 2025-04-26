#pragma once

class InputProxyServer {
public:
    void run();

private:
    int sock_fd_ = -1;

    void setup_socket();
    [[noreturn]] void accept_connections() const;
    static void handle_client(int client_fd);
};
