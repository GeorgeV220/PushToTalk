#include "InputProxyServer.h"

#include <grp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <sys/select.h>
#include <sys/stat.h>

#include "common/utilities/Utility.h"
#include "device/VirtualInputProxy.h"
#include "common/protocol/Packets.h"

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

        PacketHeader hdr{};
        std::vector<uint8_t> payload;
        if (!read_packet(client_fd, hdr, payload)) {
            throw std::runtime_error("Failed to read HAND_SHAKE packet");
        }
        if (hdr.channel != static_cast<uint16_t>(Channel::Control) ||
            hdr.type != static_cast<uint16_t>(ControlType::HAND_SHAKE)) {
            throw std::runtime_error("Expected HAND_SHAKE packet");
        }

        send_ack(client_fd);

        if (!read_packet(client_fd, hdr, payload)) {
            throw std::runtime_error("Failed to read CONFIG_LIST packet");
        }
        if (hdr.channel != static_cast<uint16_t>(Channel::Control) ||
            hdr.type != static_cast<uint16_t>(ControlType::CONFIG_LIST)) {
            throw std::runtime_error("Expected CONFIG_LIST packet");
        }

        if (payload.size() % sizeof(DeviceConfig) != 0) {
            throw std::runtime_error("Invalid CONFIG_LIST payload size");
        }

        std::vector<DeviceConfig> configs(payload.size() / sizeof(DeviceConfig));
        std::memcpy(configs.data(), payload.data(), payload.size());

        for (const auto &[vendor_id, product_id, uid, target_key, exclusive]: configs) {
            Utility::debugPrint("Config:");
            Utility::debugPrint("vendor_id: " + std::to_string(vendor_id));
            Utility::debugPrint("product_id: " + std::to_string(product_id));
            Utility::debugPrint("uid: " + std::to_string(uid));
            Utility::debugPrint("target_key: " + std::to_string(target_key));
            Utility::debugPrint("exclusive: " + std::to_string(exclusive));
        }

        send_ack(client_fd);

        VirtualInputProxy proxy;
        for (const auto &config: configs) {
            proxy.add_device(config);
        }
        proxy.set_callback([client_fd](const int key, const bool state) {
            send_key_event(client_fd, key, state);
        });
        proxy.start();

        while (true) {
            if (!read_packet(client_fd, hdr, payload)) {
                Utility::print("Client disconnected");
                break;
            }

            if (hdr.channel == static_cast<uint16_t>(Channel::Control)) {
                switch (static_cast<ControlType>(hdr.type)) {
                    case ControlType::PING:
                        write_packet(client_fd, Channel::Control, static_cast<uint16_t>(ControlType::PONG),
                                     nullptr, 0);
                        break;
                    default:
                        Utility::debugPrint("Unhandled control packet: " + std::to_string(hdr.type));
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
