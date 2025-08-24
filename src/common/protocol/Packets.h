#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

#include "common/utilities/Utility.h"

#define SOCKET_PATH "/tmp/input_proxy.sock"
#define PING_INTERVAL_MS 30000

struct sockaddr;

enum class Channel : uint16_t {
    Control = 1,
    Events = 2,
    Log = 3,
};

enum class ControlType : uint16_t {
    HAND_SHAKE = 1,
    CONFIG_LIST = 2,
    ACK = 3,
    ERROR = 4,
    PING = 5,
    PONG = 6,
};

enum class EventType : uint16_t {
    KEY_EVENT = 1,
};

struct PacketHeader {
    uint16_t channel;
    uint16_t type;
    uint32_t length;
    uint16_t flags;
};

struct KeyEventPayload {
    int32_t key{};
    uint8_t state{};
    uint8_t _pad[3]{};
};

inline std::string channel_to_string(uint16_t ch) {
    switch (static_cast<Channel>(ch)) {
        case Channel::Control: return "Control";
        case Channel::Events: return "Events";
        case Channel::Log: return "Log";
        default: return "Unknown(" + std::to_string(ch) + ")";
    }
}

inline std::string control_type_to_string(uint16_t type) {
    switch (static_cast<ControlType>(type)) {
        case ControlType::HAND_SHAKE: return "HAND_SHAKE";
        case ControlType::CONFIG_LIST: return "CONFIG_LIST";
        case ControlType::ACK: return "ACK";
        case ControlType::ERROR: return "ERROR";
        case ControlType::PING: return "PING";
        case ControlType::PONG: return "PONG";
        default: return "Unknown(" + std::to_string(type) + ")";
    }
}

inline std::string event_type_to_string(uint16_t type) {
    switch (static_cast<EventType>(type)) {
        case EventType::KEY_EVENT: return "KEY_EVENT";
        default: return "Unknown(" + std::to_string(type) + ")";
    }
}

inline int connect_to_server() {
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    while (true) {
        const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            Utility::error("socket() failed: " + std::string(strerror(errno)));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
            Utility::debugPrint("Connected to the server");
            return fd;
        }

        const int err = errno;
        close(fd);
        Utility::error("connect() failed: " + std::string(strerror(err)) + " — retrying...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

inline bool read_exact(const int fd, void *buf, const size_t n) {
    if (const auto r = Utility::safe_read(fd, buf, n); r != static_cast<ssize_t>(n)) {
        Utility::error("read_exact failed on fd=" + std::to_string(fd) +
                       " expected=" + std::to_string(n) +
                       " got=" + std::to_string(r));
        return false;
    }
    return true;
}

inline bool write_exact(const int fd, const void *buf, const size_t n) {
    if (const auto w = Utility::safe_write(fd, buf, n); w != static_cast<ssize_t>(n)) {
        Utility::error("write_exact failed on fd=" + std::to_string(fd) +
                       " expected=" + std::to_string(n) +
                       " wrote=" + std::to_string(w));
        return false;
    }
    return true;
}

inline bool write_packet(const int fd, Channel ch, const uint16_t type,
                         const void *data, const uint32_t len, const uint16_t flags = 0) {
    PacketHeader h{};
    h.channel = static_cast<uint16_t>(ch);
    h.type = type;
    h.length = len;
    h.flags = flags;

    Utility::debugPrint("Writing packet: fd=" + std::to_string(fd) +
                        " channel=" + channel_to_string(h.channel) +
                        " type=" + (h.channel == static_cast<uint16_t>(Channel::Control)
                                        ? control_type_to_string(h.type)
                                        : event_type_to_string(h.type)) +
                        " length=" + std::to_string(h.length) +
                        " flags=" + std::to_string(h.flags));

    if (!write_exact(fd, &h, sizeof(h))) {
        Utility::error("Failed to write packet header fd=" + std::to_string(fd) +
                       " channel=" + std::to_string(h.channel) +
                       " type=" + std::to_string(h.type));
        return false;
    }

    if (len > 0 && !write_exact(fd, data, len)) {
        Utility::error("Failed to write packet payload fd=" + std::to_string(fd) +
                       " channel=" + std::to_string(h.channel) +
                       " type=" + std::to_string(h.type) +
                       " length=" + std::to_string(len));
        return false;
    }

    Utility::debugPrint("Packet sent successfully fd=" + std::to_string(fd));
    return true;
}

inline bool write_packet_safe(int &fd, const Channel ch, const uint16_t type, const void *data, const uint32_t len,
                              const uint16_t flags = 0) {
    if (fd < 0) {
        fd = connect_to_server();
        if (fd < 0) return false;
    }

    if (!write_packet(fd, ch, type, data, len, flags)) {
        Utility::error("write_packet failed, trying to reconnect...");
        close(fd);
        fd = connect_to_server();
        if (fd < 0) return false;
        return write_packet(fd, ch, type, data, len, flags);
    }

    return true;
}

inline bool read_packet(const int fd, PacketHeader &hdr, std::vector<uint8_t> &payload) {
    Utility::debugPrint("Reading packet header from fd=" + std::to_string(fd));

    if (!read_exact(fd, &hdr, sizeof(hdr))) {
        Utility::error("Failed to read packet header fd=" + std::to_string(fd));
        return false;
    }

    Utility::debugPrint("Header read fd=" + std::to_string(fd) +
                        " channel=" + channel_to_string(hdr.channel) +
                        " type=" + (hdr.channel == static_cast<uint16_t>(Channel::Control)
                                        ? control_type_to_string(hdr.type)
                                        : event_type_to_string(hdr.type)) +
                        " length=" + std::to_string(hdr.length) +
                        " flags=" + std::to_string(hdr.flags));

    payload.resize(hdr.length);
    if (hdr.length == 0 && hdr.channel != static_cast<uint16_t>(Channel::Control)) {
        Utility::debugPrint("Packet has no payload fd=" + std::to_string(fd));
        return true;
    }

    Utility::debugPrint("Reading packet payload fd=" + std::to_string(fd) +
                        " length=" + std::to_string(hdr.length));

    if (!read_exact(fd, payload.data(), hdr.length)) {
        Utility::error("Failed to read packet payload fd=" + std::to_string(fd) +
                       " channel=" + std::to_string(hdr.channel) +
                       " type=" + std::to_string(hdr.type) +
                       " expected=" + std::to_string(hdr.length));
        return false;
    }

    Utility::debugPrint("Packet payload read successfully fd=" + std::to_string(fd));
    return true;
}

inline void send_ack(int fd) {
    Utility::debugPrint("Sending ACK to fd=" + std::to_string(fd));
    write_packet_safe(fd, Channel::Control, static_cast<uint16_t>(ControlType::ACK), nullptr, 0);
}

inline void send_error(int fd, const std::string &msg) {
    Utility::debugPrint("Sending ERROR to fd=" + std::to_string(fd) +
                        " msg=" + msg);
    write_packet_safe(fd, Channel::Control, static_cast<uint16_t>(ControlType::ERROR),
                      msg.data(), static_cast<uint32_t>(msg.size()));
}

inline void send_key_event(int fd, const int key, const bool state) {
    Utility::debugPrint("Sending KEY_EVENT to fd=" + std::to_string(fd) +
                        " key=" + std::to_string(key) +
                        " state=" + std::to_string(state));
    const KeyEventPayload p{key, static_cast<uint8_t>(state ? 1 : 0)};
    write_packet_safe(fd, Channel::Events, static_cast<uint16_t>(EventType::KEY_EVENT), &p, sizeof(p));
}
