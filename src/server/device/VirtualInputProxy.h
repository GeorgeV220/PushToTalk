#ifndef VIRTUALINPUTPROXY_H
#define VIRTUALINPUTPROXY_H

#include "../../common/device/DeviceCapabilities.h"
#include <functional>
#include <thread>
#include <atomic>
#include <string>
#include <linux/uinput.h>

class VirtualInputProxy {
public:
    VirtualInputProxy(uint16_t vendor_id, uint16_t product_id, uint32_t uid, int target_key);

    ~VirtualInputProxy();

    void set_callback(std::function<void(bool)> callback);

    void start();

    void stop();

    static void detect_devices();

private:
    int fd_physical_ = -1;
    int ufd_ = -1;
    int target_key_;
    std::atomic<bool> running_{false};
    std::thread listener_thread_;
    std::function<void(bool)> callback_;

    void create_virtual_device();

    void setup_capabilities() const;

    void setup_event_codes(int ev_type) const;

    void handle_event(const input_event &ev) const;

    static int get_max_code(int ev_type);

    void set_virtual_bit(int ev_type, int code) const;

    static std::string find_device_path(uint16_t vendor_id, uint16_t product_id, uint32_t expected_uid);
};

#endif // VIRTUALINPUTPROXY_H
