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

    ~VirtualInputProxy();

    void add_device(const DeviceConfig &config);

    void remove_device(const DeviceConfig &config);

    void set_callback(Callback callback);

    void start_retry_loop();
    void stop_retry_loop();

    void start();

    void stop();

    static void detect_devices();

private:
    std::thread retry_thread;
    std::atomic<bool> running = false;
    std::vector<DeviceConfig> failed_configs = {};

    struct DeviceContext {
        int fd_physical = -1;
        int ufd = -1;
        int target_key = -1;
        bool exclusive = false;
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

    void add_failed_config(const DeviceConfig &config);

    void remove_failed_config(const DeviceConfig &config);

    void retry_failed_configs();

    void handle_event(const DeviceContext &ctx, const input_event &ev) const;

    [[nodiscard]] static int create_virtual_device(int physical_fd);

    static bool setup_capabilities(int physical_fd, int ufd);

    static void setup_event_codes(int physical_fd, int ufd, int ev_type);

    static void set_virtual_bit(int ufd, int ev_type, int code, int physical_fd);

    static std::string find_device_path(uint16_t vendor_id, uint16_t product_id, uint32_t expected_uid);

    static int get_max_code(int ev_type);
};

#endif // VIRTUALINPUTPROXY_H
