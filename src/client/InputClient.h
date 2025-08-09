#ifndef INPUTCLIENT_H
#define INPUTCLIENT_H

#include <functional>
#include <thread>

struct DeviceConfig;

class InputClient {
public:
    ~InputClient();

    void start();

    void stop();

    void restart();

    void set_callback(std::function<void(bool)> callback);

    void clear_devices();

    void add_device(uint16_t vendor_id, uint16_t product_id, uint32_t uid, int target_key, bool exclusive = false);

private:
    std::vector<DeviceConfig> configs_;

    int sock_fd_ = -1;
    std::thread listener_thread_;
    std::atomic<bool> running_{false};
    std::function<void(bool)> callback_;
};

#endif //INPUTCLIENT_H
