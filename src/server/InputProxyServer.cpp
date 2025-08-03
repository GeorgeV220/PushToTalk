#include "InputProxyServer.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <sys/select.h>
#include <sys/stat.h>

#include "common/utilities/Utility.h"
#include "device/VirtualInputProxy.h"

#define SOCKET_PATH "/tmp/input_proxy.sock"

void InputProxyServer::run() {
    setup_socket();
    accept_connections();
}

void InputProxyServer::setup_socket() {
    sockaddr_un addr = {};

    if ((sock_fd_ = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        throw std::runtime_error("Socket creation failed: " + std::string(strerror(errno)));
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH);
    if (bind(sock_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr))) {
        shutdown(sock_fd_, SHUT_RDWR);
        close(sock_fd_);
        throw std::runtime_error("Bind failed: " + std::string(strerror(errno)));
    }

    const UserInfo user = Utility::get_active_user_info();

    if (chown(SOCKET_PATH, user.uid, user.gid) < 0) {
        throw std::runtime_error("chown failed: " + std::string(strerror(errno)));
    }
    Utility::print("Socket ownership changed to " + std::to_string(user.uid) + ":" + std::to_string(user.gid));

    if (chmod(SOCKET_PATH, 0660) < 0) {
        throw std::runtime_error("chmod failed: " + std::string(strerror(errno)));
    }

    if (listen(sock_fd_, 5)) {
        shutdown(sock_fd_, SHUT_RDWR);
        close(sock_fd_);
        throw std::runtime_error("Listen failed: " + std::string(strerror(errno)));
    }
}

[[noreturn]] void InputProxyServer::accept_connections() const {
    while (true) {
        sockaddr_un client_addr{};
        socklen_t client_len = sizeof(client_addr);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd_, &read_fds);

        if (const int result = select(sock_fd_ + 1, &read_fds, nullptr, nullptr, nullptr); result < 0) {
            std::cerr << "select() failed: " << strerror(errno) << std::endl;
            continue;
        }

        if (FD_ISSET(sock_fd_, &read_fds)) {
            const int client_fd = accept(sock_fd_,
                                         reinterpret_cast<struct sockaddr *>(&client_addr),
                                         &client_len);
            if (client_fd < 0) {
                continue;
            }

            handle_client(client_fd);
            close(client_fd);
        }
    }
}

void InputProxyServer::handle_client(int client_fd) {
    try {
        ucred cred{};
        socklen_t len = sizeof(cred);

        if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len)) {
            throw std::runtime_error("Failed to get client credentials: " + std::string(strerror(errno)));
        }

        if (cred.uid != Utility::get_active_user_info().uid) {
            throw std::runtime_error("Unauthorized client UID");
        }

        uint32_t count;
        if (Utility::safe_read(client_fd, &count, sizeof(count)) != sizeof(count)) {
            throw std::runtime_error("Failed to read config count");
        }

        if (count > 1000) {
            throw std::runtime_error("Too many configs from client");
        }

        InitParams params = {};
        params.configs.resize(count);

        for (uint32_t i = 0; i < count; ++i) {
            std::cout << "Reading config " << i << std::endl;
            if (Utility::safe_read(client_fd, &params.configs[i], sizeof(DeviceConfig)) != sizeof(DeviceConfig)) {
                throw std::runtime_error("Failed to read device config");
            }
        }

        for (const auto &[vendor_id, product_id, uid, target_key]: params.configs) {
            Utility::debugPrint("Config:");
            Utility::debugPrint("vendor_id: " + std::to_string(vendor_id));
            Utility::debugPrint("product_id: " + std::to_string(product_id));
            Utility::debugPrint("uid: " + std::to_string(uid));
            Utility::debugPrint("target_key: " + std::to_string(target_key));
        }
        const std::vector configs = {
            params.configs
        };

        VirtualInputProxy proxy(configs);
        proxy.set_callback([client_fd](const int key, const bool state) {
            std::cout << "Sending event for key " << key << " to client " << client_fd << std::endl;
            if (Utility::safe_write(client_fd, &state, sizeof(state)) != sizeof(state)) {
                throw std::runtime_error("Client write failed");
            }
        });
        proxy.start();

        fd_set read_fds;
        char buf;
        while (true) {
            FD_ZERO(&read_fds);
            FD_SET(client_fd, &read_fds);

            if (const int ret = select(client_fd + 1, &read_fds, nullptr, nullptr, nullptr); ret < 0) {
                std::cerr << "select() failed: " << strerror(errno) << std::endl;
                break;
            }

            if (FD_ISSET(client_fd, &read_fds)) {
                const ssize_t n = Utility::safe_read(client_fd, &buf, sizeof(buf));
                if (n == 0) {
                    break;
                }
                if (n < 0) {
                    std::cerr << "Error reading from client: " << strerror(errno) << std::endl;
                    break;
                }
            }
        }

        shutdown(client_fd, SHUT_RDWR);
    } catch (const std::exception &e) {
        std::cerr << "Client handling error: " << e.what() << std::endl;
    }

    close(client_fd);
}
