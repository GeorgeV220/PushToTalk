#include "InputClient.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <functional>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>

#include "common/utilities/Utility.h"

#define SOCKET_PATH "/tmp/input_proxy.sock"

InputClient::~InputClient() {
    stop();
}

void InputClient::set_callback(std::function<void(bool)> callback) {
    callback_ = std::move(callback);
}

void InputClient::add_device(const uint16_t vendor_id, const uint16_t product_id, const uint32_t uid,
                             const int target_key) {
    const DeviceConfig config = {vendor_id, product_id, uid, target_key};
    configs_.push_back(config);
}


void InputClient::start() {
    if (running_) {
        throw std::runtime_error("Client already running");
    }
    if (!callback_) {
        throw std::runtime_error("Callback not set");
    }

    sockaddr_un addr = {};
    sock_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd_ < 0) {
        throw std::runtime_error("Socket creation failed");
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (::connect(sock_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        const int err = errno;
        shutdown(sock_fd_, SHUT_RDWR);
        close(sock_fd_);
        throw std::runtime_error(
            "Connection failed: " +
            std::string(strerror(err)) +
            " (errno=" + std::to_string(err) + ")"
        );
    }


    InitParams params;
    params.configs = configs_;

    for (const auto &[vendor_id, product_id, uid, target_key]: configs_) {
        Utility::debugPrint("Config:");
        Utility::debugPrint("vendor_id: " + std::to_string(vendor_id));
        Utility::debugPrint("product_id: " + std::to_string(product_id));
        Utility::debugPrint("uid: " + std::to_string(uid));
        Utility::debugPrint("target key: " + std::to_string(target_key));
    }

    const auto count = static_cast<uint32_t>(params.configs.size());
    if (Utility::safe_write(sock_fd_, &count, sizeof(count)) != sizeof(count)) {
        shutdown(sock_fd_, SHUT_RDWR);
        close(sock_fd_);
        throw std::runtime_error("Parameter count send failed");
    }

    for (const auto &cfg: params.configs) {
        if (Utility::safe_write(sock_fd_, &cfg, sizeof(DeviceConfig)) != sizeof(DeviceConfig)) {
            shutdown(sock_fd_, SHUT_RDWR);
            close(sock_fd_);
            throw std::runtime_error("Parameter config send failed");
        }
    }

    running_ = true;
    listener_thread_ = std::thread([this]() {
        bool state;
        while (running_) {
            if (Utility::safe_read(sock_fd_, &state, sizeof(state)) == sizeof(state)) {
                callback_(state);
            } else {
                running_ = false;
            }
        }
        shutdown(sock_fd_, SHUT_RDWR);
        close(sock_fd_);
        sock_fd_ = -1;
    });
}

void InputClient::stop() {
    running_ = false;
    if (sock_fd_ >= 0) {
        shutdown(sock_fd_, SHUT_RDWR);
        close(sock_fd_);
        sock_fd_ = -1;
    }
    Utility::print("Stopping client");
    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }
    Utility::print("Client stopped");
}

void InputClient::restart() {
    stop();
    start();
}
