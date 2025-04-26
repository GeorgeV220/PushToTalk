#include "InputClient.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <functional>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>

#include "../common/utilities/Utility.h"

#define SOCKET_PATH "/tmp/input_proxy.sock"

InputClient::~InputClient() {
    stop();
}

void InputClient::set_vendor_id(const uint16_t vendor_id) {
    vendor_id_ = vendor_id;
}

void InputClient::set_product_id(const uint16_t product_id) {
    product_id_ = product_id;
}

void InputClient::set_device_uid(const uint32_t uid) {
    uid_ = uid;
}

void InputClient::set_target_key(const int target_key) {
    target_key_ = target_key;
}

void InputClient::set_callback(std::function<void(bool)> callback) {
    callback_ = std::move(callback);
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

    const struct InitParams {
        uint16_t vendor_id;
        uint16_t product_id;
        uint32_t uid;
        int target_key;
    } params = {
                vendor_id_,
                product_id_,
                uid_,
                target_key_
            };

    Utility::debugPrint("Sending Data:");
    std::ostringstream vendor_oss;
    vendor_oss << "vendor_id: " << std::hex << params.vendor_id;
    Utility::debugPrint(vendor_oss.str());
    std::ostringstream product_oss;
    product_oss << "product_id: " << std::hex << params.product_id;
    Utility::debugPrint(product_oss.str());
    std::ostringstream uid_oss;
    uid_oss << "uid: " << std::hex << params.uid;
    Utility::debugPrint(uid_oss.str());
    Utility::debugPrint("target key: " + std::to_string(target_key_));

    if (Utility::safe_write(sock_fd_, &params, sizeof(params)) != sizeof(params)) {
        shutdown(sock_fd_, SHUT_RDWR);
        close(sock_fd_);
        throw std::runtime_error("Parameter send failed");
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
