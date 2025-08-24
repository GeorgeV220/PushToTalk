#include "InputClient.h"
#include "common/utilities/Utility.h"
#include "common/protocol/Packets.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <atomic>

InputClient::~InputClient() {
    stop();
}

void InputClient::set_callback(std::function<void(bool)> callback) {
    callback_ = std::move(callback);
}

void InputClient::clear_devices() {
    configs_.clear();
}

void InputClient::add_device(const uint16_t vendor_id, const uint16_t product_id,
                             const uint32_t uid, const int target_key, const bool exclusive) {
    configs_.push_back({vendor_id, product_id, uid, target_key, exclusive});
}

bool InputClient::wait_for_ack(const int fd) {
    PacketHeader hdr{};
    if (std::vector<uint8_t> payload; !read_packet(fd, hdr, payload)) return false;

    if (hdr.channel == static_cast<uint16_t>(Channel::Control) &&
        hdr.type == static_cast<uint16_t>(ControlType::ACK)) {
        return true;
    }

    Utility::error("Expected ACK but got type=" + std::to_string(hdr.type));
    return false;
}

int InputClient::connect_and_handshake() const {
    int fd = connect_to_server();

    write_packet_safe(fd, Channel::Control,
                      static_cast<uint16_t>(ControlType::HAND_SHAKE),
                      nullptr, 0);
    if (!wait_for_ack(fd)) throw std::runtime_error("HAND_SHAKE not acknowledged by server");

    if (!configs_.empty()) {
        write_packet_safe(fd, Channel::Control,
                          static_cast<uint16_t>(ControlType::CONFIG_LIST),
                          configs_.data(),
                          configs_.size() * sizeof(DeviceConfig));
        if (!wait_for_ack(fd)) throw std::runtime_error("CONFIG_LIST not acknowledged by server");
    }

    return fd;
}

void InputClient::start() {
    if (running_) throw std::runtime_error("Client already running");
    if (!callback_) throw std::runtime_error("Callback not set");

    sock_fd_ = connect_and_handshake();
    running_ = true;

    listener_thread_ = std::thread([this]() {
        PacketHeader hdr{};
        std::vector<uint8_t> payload;
        std::atomic pong_received{true};
        std::atomic pong_missed{0};

        std::thread ping_thread([this, &pong_received, &pong_missed]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(PING_INTERVAL_MS));
                if (!running_) break;

                if (!write_packet_safe(sock_fd_, Channel::Control,
                                       static_cast<uint16_t>(ControlType::PING),
                                       nullptr, 0)) {
                    Utility::error("Ping failed, will reconnect on next read");
                }

                pong_received = false;
                if (pong_missed > 3) restart();
                ++pong_missed;
            }
        });

        while (running_) {
            if (!read_packet(sock_fd_, hdr, payload)) {
                Utility::error("Read failed — reconnecting...");

                if (sock_fd_ >= 0) {
                    shutdown(sock_fd_, SHUT_RDWR);
                    close(sock_fd_);
                    sock_fd_ = -1;
                }

                try {
                    sock_fd_ = connect_and_handshake();
                    pong_received = true;
                    pong_missed = 0;
                } catch (const std::exception &e) {
                    Utility::error("Reconnect failed: " + std::string(e.what()));
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                continue;
            }

            if (hdr.channel == static_cast<uint16_t>(Channel::Events) &&
                hdr.type == static_cast<uint16_t>(EventType::KEY_EVENT) &&
                payload.size() == sizeof(KeyEventPayload)) {
                KeyEventPayload ev{};
                std::memcpy(&ev, payload.data(), sizeof(ev));
                callback_(ev.state != 0);
            } else if (hdr.channel == static_cast<uint16_t>(Channel::Control)) {
                switch (static_cast<ControlType>(hdr.type)) {
                    case ControlType::PONG:
                        pong_received = true;
                        pong_missed = 0;
                        break;
                    case ControlType::ERROR: {
                        std::string msg(payload.begin(), payload.end());
                        Utility::error("Server error: " + msg);
                        break;
                    }
                    case ControlType::ACK:
                        Utility::debugPrint("Received ACK");
                        break;
                    default:
                        Utility::debugPrint("Unhandled control packet: " + std::to_string(hdr.type));
                        break;
                }
            }
        }

        running_ = false;
        if (ping_thread.joinable()) ping_thread.join();

        if (sock_fd_ >= 0) {
            shutdown(sock_fd_, SHUT_RDWR);
            close(sock_fd_);
            sock_fd_ = -1;
        }
    });
}

void InputClient::stop() {
    running_ = false;
    if (sock_fd_ >= 0) {
        shutdown(sock_fd_, SHUT_RDWR);
        close(sock_fd_);
        sock_fd_ = -1;
    }
    if (listener_thread_.joinable()) listener_thread_.join();
}

void InputClient::restart() {
    stop();
    sock_fd_ = connect_and_handshake();
    running_ = true;
}
