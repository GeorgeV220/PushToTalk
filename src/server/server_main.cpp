#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <sstream>

#include "../common/utilities/Utility.h"
#include "device/VirtualInputProxy.h"

#define SOCKET_PATH "/tmp/input_proxy.sock"

class InputProxyServer {
public:
    void run() {
        setup_socket();
        accept_connections();
    }

private:
    int sock_fd_ = -1;

    void setup_socket() {
        sockaddr_un addr = {};

        if ((sock_fd_ = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            throw std::runtime_error("Socket creation failed");
        }

        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

        unlink(SOCKET_PATH);
        if (bind(sock_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr))) {
            close(sock_fd_);
            throw std::runtime_error("Bind failed");
        }

        UserInfo user = Utility::get_active_user_info();

        if (chown(SOCKET_PATH, user.uid, user.gid) < 0) {
            throw std::runtime_error("chown failed: " + std::string(strerror(errno)));
        }
        Utility::print("Socket ownership changed to " + std::to_string(user.uid) + ":" + std::to_string(user.gid));

        if (listen(sock_fd_, 5)) {
            close(sock_fd_);
            throw std::runtime_error("Listen failed");
        }
    }

    [[noreturn]] void accept_connections() const {
        while (true) {
            sockaddr_un client_addr{};
            socklen_t client_len = sizeof(client_addr);

            const int client_fd = accept(sock_fd_,
                                         reinterpret_cast<struct sockaddr *>(&client_addr),
                                         &client_len);
            if (client_fd < 0) continue;

            handle_client(client_fd);
            close(client_fd);
        }
    }

    static void handle_client(int client_fd) {
        try {
            ucred cred{};
            socklen_t len = sizeof(cred);

            if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len)) {
                throw std::runtime_error("Failed to get client credentials");
            }

            if (cred.uid != Utility::get_active_user_info().uid) {
                throw std::runtime_error("Unauthorized client UID");
            }

            struct InitParams {
                uint16_t vendor_id;
                uint16_t product_id;
                uint32_t uid;
                int target_key;
            } params{};

            if (read(client_fd, &params, sizeof(params)) != sizeof(params)) {
                throw std::runtime_error("Invalid init parameters");
            }

            Utility::debugPrint("Received Data:");
            std::ostringstream vendor_oss;
            vendor_oss << std::hex << params.vendor_id;
            Utility::debugPrint("vendor_id: " + vendor_oss.str());

            std::ostringstream product_oss;
            product_oss << std::hex << params.product_id;
            Utility::debugPrint("product_id: " + product_oss.str());

            std::ostringstream uid_oss;
            uid_oss << std::hex << params.uid;
            Utility::debugPrint("uid: " + uid_oss.str());

            Utility::debugPrint("target_key: " + std::to_string(params.target_key));

            // Use params as usual after conversion
            VirtualInputProxy vip(
                params.vendor_id,
                params.product_id,
                params.uid,
                params.target_key
            );

            vip.set_callback([client_fd](bool state) {
                if (write(client_fd, &state, sizeof(state)) != sizeof(state)) {
                    // Handle client disconnect
                    throw std::runtime_error("Client write failed");
                }
            });

            vip.start();

            char buf;
            while (read(client_fd, &buf, 1) > 0) {
            }
        } catch (const std::exception &e) {
            std::cerr << "Client handling error: " << e.what() << std::endl;
        }
    }
};

int main(const int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "--debug") {
            Utility::debug = true;
        } else if (arg == "--detect") {
            VirtualInputProxy::detect_devices();
        }
    }
    try {
        InputProxyServer server;
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Server fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
