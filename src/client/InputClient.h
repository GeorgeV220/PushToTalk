#ifndef INPUTCLIENT_H
#define INPUTCLIENT_H

#include <functional>
#include <thread>

class InputClient {
public:
    ~InputClient();

    void start();

    void stop();

    void restart();

    void set_callback(std::function<void(bool)> callback);

    void set_vendor_id(uint16_t vendor_id);

    void set_product_id(uint16_t product_id);

    void set_device_uid(uint32_t uid);

    void set_target_key(int target_key);

private:
    uint16_t vendor_id_ = 0;
    uint16_t product_id_ = 0;
    uint32_t uid_ = 0;
    int target_key_ = -1;

    int sock_fd_ = -1;
    std::thread listener_thread_;
    std::atomic<bool> running_{false};
    std::function<void(bool)> callback_;
};

#endif //INPUTCLIENT_H
