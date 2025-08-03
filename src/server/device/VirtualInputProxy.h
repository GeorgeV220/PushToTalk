#ifndef VIRTUALINPUTPROXY_H
#define VIRTUALINPUTPROXY_H

#include <functional>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <linux/input.h>

#include "common/device/DeviceCapabilities.h"
#include "common/utilities/Utility.h"

class VirtualInputProxy {
public:
    using Callback = std::function<void(int key, bool state)>;

    explicit VirtualInputProxy(const std::vector<DeviceConfig> &configs);

    ~VirtualInputProxy();

    void set_callback(Callback callback);

    void start();

    void stop();

    static void detect_devices();

private:
    struct DeviceContext {
        int fd_physical = -1;
        int ufd = -1;
        int target_key = -1;
        std::atomic<bool> running{false};
        std::thread listener_thread;

        DeviceContext() = default;

        DeviceContext(DeviceContext &&other) noexcept
            : fd_physical(other.fd_physical),
              ufd(other.ufd),
              target_key(other.target_key),
              running(other.running.load()),
              listener_thread(std::move(other.listener_thread)) {
        }

        DeviceContext &operator=(DeviceContext &&other) noexcept {
            if (this != &other) {
                fd_physical = other.fd_physical;
                ufd = other.ufd;
                target_key = other.target_key;
                running.store(other.running.load());
                listener_thread = std::move(other.listener_thread);
            }
            return *this;
        }

        DeviceContext(const DeviceContext &) = delete;

        DeviceContext &operator=(const DeviceContext &) = delete;
    };


    std::vector<DeviceContext> contexts_;
    Callback callback_;

    void handle_event(DeviceContext &ctx, const input_event &ev) const;

    [[nodiscard]] static int create_virtual_device(int physical_fd);

    static void setup_capabilities(int physical_fd, int ufd);

    static void setup_event_codes(int physical_fd, int ufd, int ev_type);

    static void set_virtual_bit(int ufd, int ev_type, int code);

    static std::string find_device_path(uint16_t vendor_id, uint16_t product_id, uint32_t expected_uid);

    static int get_max_code(int ev_type);
};

#endif // VIRTUALINPUTPROXY_H
