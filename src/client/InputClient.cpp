#include <sys/socket.h>
#include <sys/un.h>
#include <functional>
#include <thread>
#include <atomic>
#include <stdexcept>
#include <cstring>

#define SOCKET_PATH "/tmp/input_proxy.sock"

class InputClient {
public:
    explicit InputClient(const uint16_t vendor_id,
                         const uint16_t product_id,
                         const uint32_t uid,
                         const int target_key)
        : vendor_id_(vendor_id),
          product_id_(product_id),
          uid_(uid),
          target_key_(target_key) {
    }

    void set_callback(std::function<void(bool)> callback) {
        callback_ = std::move(callback);
    }

    void start() {
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

        if (::connect(sock_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            const int err = errno;
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
        } params = {vendor_id_, product_id_, uid_, target_key_};

        if (write(sock_fd_, &params, sizeof(params)) != sizeof(params)) {
            close(sock_fd_);
            throw std::runtime_error("Parameter send failed");
        }

        running_ = true;
        listener_thread_ = std::thread([this]() {
            bool state;
            while (running_) {
                if (read(sock_fd_, &state, sizeof(state)) == sizeof(state)) {
                    callback_(state);
                } else {
                    running_ = false;
                }
            }
            close(sock_fd_);
            sock_fd_ = -1;
        });
    }

    void stop() {
        running_ = false;
        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }
        if (sock_fd_ >= 0) {
            close(sock_fd_);
            sock_fd_ = -1;
        }
    }

    ~InputClient() {
        stop();
    }

private:
    const uint16_t vendor_id_;
    const uint16_t product_id_;
    const uint32_t uid_;
    const int target_key_;

    int sock_fd_ = -1;
    std::thread listener_thread_;
    std::atomic<bool> running_{false};
    std::function<void(bool)> callback_;
};
