#include "InputProxyServer.h"

#include <grp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <sys/select.h>
#include <sys/stat.h>

#include "common/utilities/Utility.h"
#include "device/VirtualInputProxy.h"

#define SOCKET_PATH "/tmp/input_proxy.sock"
#define CONTROL_GROUP "ptt"


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
    if (bind(sock_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(sock_fd_);
        throw std::runtime_error("Bind failed: " + std::string(strerror(errno)));
    }

    const group *grp = getgrnam(CONTROL_GROUP);
    if (!grp) {
        Utility::print("Group '" CONTROL_GROUP "' not found, creating it...");
        if (const int ret = system("groupadd " CONTROL_GROUP); ret != 0) {
            close(sock_fd_);
            throw std::runtime_error("Failed to create group '" CONTROL_GROUP "'");
        }
        grp = getgrnam(CONTROL_GROUP);
        if (!grp) {
            close(sock_fd_);
            throw std::runtime_error("Group '" CONTROL_GROUP "' creation failed");
        }
    }

    if (chown(SOCKET_PATH, -1, grp->gr_gid) < 0) {
        close(sock_fd_);
        throw std::runtime_error("chown failed: " + std::string(strerror(errno)));
    }

    if (chmod(SOCKET_PATH, 0660) < 0) {
        close(sock_fd_);
        throw std::runtime_error("chmod failed: " + std::string(strerror(errno)));
    }

    if (listen(sock_fd_, 5) < 0) {
        close(sock_fd_);
        throw std::runtime_error("Listen failed: " + std::string(strerror(errno)));
    }
    Utility::print("Listening on " SOCKET_PATH);
}

[[noreturn]] void InputProxyServer::accept_connections() const {
    while (true) {
        sockaddr_un client_addr{};
        socklen_t client_len = sizeof(client_addr);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd_, &read_fds);

        if (const int result = select(sock_fd_ + 1, &read_fds, nullptr, nullptr, nullptr); result < 0) {
            Utility::error("select() failed: " + std::string(strerror(errno)));
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
            Utility::debugPrint("Reading config " + std::to_string(i));
            if (Utility::safe_read(client_fd, &params.configs[i], sizeof(DeviceConfig)) != sizeof(DeviceConfig)) {
                throw std::runtime_error("Failed to read device config");
            }
        }

        for (const auto &[vendor_id, product_id, uid, target_key, exclusive]: params.configs) {
            Utility::debugPrint("Config:");
            Utility::debugPrint("vendor_id: " + std::to_string(vendor_id));
            Utility::debugPrint("product_id: " + std::to_string(product_id));
            Utility::debugPrint("uid: " + std::to_string(uid));
            Utility::debugPrint("target_key: " + std::to_string(target_key));
            Utility::debugPrint("exclusive: " + std::to_string(exclusive));
        }
        const std::vector configs = {
            params.configs
        };

        VirtualInputProxy proxy;
        for (const auto &config: configs) {
            proxy.add_device(config);
        }
        proxy.set_callback([client_fd](const int key, const bool state) {
            Utility::debugPrint(
                "Sending event for key " + std::to_string(key) + " to client " + std::to_string(client_fd));
            if (Utility::safe_write(client_fd, &state, sizeof(state)) != sizeof(state)) {
                Utility::error(
                    "Failed to send event to client " + std::to_string(client_fd) + ": " + std::string(
                        strerror(errno)));
            }
        });
        proxy.start();

        fd_set read_fds;
        char buf;
        while (true) {
            FD_ZERO(&read_fds);
            FD_SET(client_fd, &read_fds);

            if (const int ret = select(client_fd + 1, &read_fds, nullptr, nullptr, nullptr); ret < 0) {
                Utility::error("select() failed: " + std::string(strerror(errno)));
                break;
            }

            if (FD_ISSET(client_fd, &read_fds)) {
                const ssize_t n = Utility::safe_read(client_fd, &buf, sizeof(buf));
                if (n == 0) {
                    break;
                }
                if (n < 0) {
                    Utility::error("Error reading from client: " + std::string(strerror(errno)));
                    break;
                }
            }
        }

        shutdown(client_fd, SHUT_RDWR);
    } catch (const std::exception &e) {
        Utility::error("Client handling error: " + std::string(e.what()));
    }

    close(client_fd);
}
