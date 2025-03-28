#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <functional>
#include <thread>
#include <atomic>
#include <stdexcept>
#include <string>
#include <cstring>

class VirtualInputProxy {
public:
    VirtualInputProxy(const std::string &device_path, int target_key)
        : target_key_(target_key), running_(false) {
        fd_physical_ = open(device_path.c_str(), O_RDWR);
        if (fd_physical_ < 0) {
            throw std::runtime_error("Failed to open input device");
        }

        if (ioctl(fd_physical_, EVIOCGRAB, 1) < 0) {
            close(fd_physical_);
            throw std::runtime_error("Failed to grab physical device");
        }

        create_virtual_device();
    }

    ~VirtualInputProxy() {
        stop();
        if (ufd_ >= 0) {
            ioctl(ufd_, UI_DEV_DESTROY);
            close(ufd_);
        }
        if (fd_physical_ >= 0) {
            ioctl(fd_physical_, EVIOCGRAB, 0);
            close(fd_physical_);
        }
    }

    void set_callback(std::function<void(bool)> callback) {
        callback_ = callback;
    }

    void start() {
        if (running_) return;

        running_ = true;
        listener_thread_ = std::thread([this]() {
            input_event ev;
            while (running_) {
                if (const ssize_t bytes = read(fd_physical_, &ev, sizeof(ev)); bytes == sizeof(ev)) {
                    handle_event(ev);
                } else if (bytes < 0) {
                    if (errno != EAGAIN && errno != EINTR) break;
                }
            }
        });
    }

    void stop() {
        running_ = false;
        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }
    }

private:
    int fd_physical_ = -1;
    int ufd_ = -1;
    int target_key_;
    std::atomic<bool> running_;
    std::thread listener_thread_;
    std::function<void(bool)> callback_;

    void create_virtual_device() {
        ufd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (ufd_ < 0) {
            throw std::runtime_error("Failed to open uinput");
        }

        setup_capabilities();

        uinput_user_dev uidev = {};
        strncpy(uidev.name, "PTT Virtual Device", UINPUT_MAX_NAME_SIZE);
        uidev.id.bustype = BUS_VIRTUAL;

        if (write(ufd_, &uidev, sizeof(uidev)) != sizeof(uidev)) {
            close(ufd_);
            throw std::runtime_error("Failed to write uinput config");
        }

        if (ioctl(ufd_, UI_DEV_CREATE) < 0) {
            close(ufd_);
            throw std::runtime_error("Failed to create virtual device");
        }
    }

    void setup_capabilities() const {
        unsigned long evtypes[EV_MAX / 8 + 1] = {0};
        if (ioctl(fd_physical_, EVIOCGBIT(0, EV_MAX), evtypes) < 0) {
            close(ufd_);
            throw std::runtime_error("Failed to get event types");
        }

        for (int ev = 0; ev < EV_MAX; ++ev) {
            if (test_bit(ev, evtypes)) {
                ioctl(ufd_, UI_SET_EVBIT, ev);
                setup_event_codes(ev);
            }
        }
    }

    void setup_event_codes(const int ev_type) const {
        unsigned long codes[KEY_MAX / 8 + 1] = {0};
        const int max_code = get_max_code(ev_type);

        if (ioctl(fd_physical_, EVIOCGBIT(ev_type, max_code), codes) < 0) {
            return;
        }

        for (int code = 0; code < max_code; ++code) {
            if (test_bit(code, codes)) {
                set_virtual_bit(ev_type, code);
            }
        }
    }

    static int get_max_code(const int ev_type) {
        switch (ev_type) {
            case EV_KEY: return KEY_MAX;
            case EV_REL: return REL_MAX;
            case EV_ABS: return ABS_MAX;
            case EV_MSC: return MSC_MAX;
            case EV_LED: return LED_MAX;
            default: return 0;
        }
    }

    void set_virtual_bit(const int ev_type, const int code) const {
        switch (ev_type) {
            case EV_KEY: ioctl(ufd_, UI_SET_KEYBIT, code);
                break;
            case EV_REL: ioctl(ufd_, UI_SET_RELBIT, code);
                break;
            case EV_ABS: ioctl(ufd_, UI_SET_ABSBIT, code);
                break;
            case EV_MSC: ioctl(ufd_, UI_SET_MSCBIT, code);
                break;
            case EV_LED: ioctl(ufd_, UI_SET_LEDBIT, code);
                break;
        }
    }

    void handle_event(const input_event &ev) const {
        if (ev.type == EV_KEY && ev.code == target_key_) {
            if (callback_) callback_(ev.value);
        } else {
            write(ufd_, &ev, sizeof(ev));
            constexpr input_event syn = {.time = {}, .type = EV_SYN, .code = SYN_REPORT, .value = 0};
            write(ufd_, &syn, sizeof(syn));
        }
    }

    static bool test_bit(const int bit, const unsigned long *arr) {
        return arr[bit / (sizeof(long) * 8)] & (1UL << (bit % (sizeof(long) * 8)));
    }
};
