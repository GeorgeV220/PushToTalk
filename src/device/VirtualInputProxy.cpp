#include "VirtualInputProxy.h"
#include "DeviceCapabilities.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <stdexcept>
#include <utility>

using namespace DeviceUtils;

VirtualInputProxy::VirtualInputProxy(
    const uint16_t vendor_id,
    const uint16_t product_id,
    const uint32_t uid,
    const int target_key)
    : target_key_(target_key) {
    const std::string device_path = find_device_path(vendor_id, product_id, uid);
    fd_physical_ = open(device_path.c_str(), O_RDWR);
    if (fd_physical_ < 0) throw std::runtime_error("Failed to open input device");

    if (ioctl(fd_physical_, EVIOCGRAB, 1) < 0) {
        close(fd_physical_);
        throw std::runtime_error("Failed to grab physical device");
    }

    create_virtual_device();
}

VirtualInputProxy::~VirtualInputProxy() {
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

void VirtualInputProxy::set_callback(std::function<void(bool)> callback) {
    callback_ = std::move(callback);
}

void VirtualInputProxy::start() {
    if (running_) return;

    running_ = true;
    listener_thread_ = std::thread([this]() {
        input_event ev{};
        while (running_) {
            if (const ssize_t bytes = read(fd_physical_, &ev, sizeof(ev)); bytes == sizeof(ev)) {
                handle_event(ev);
            } else if (bytes < 0) {
                if (errno != EAGAIN && errno != EINTR) break;
            }
        }
    });
}

void VirtualInputProxy::stop() {
    running_ = false;
    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }
}

std::string VirtualInputProxy::find_device_path(const uint16_t vendor_id, const uint16_t product_id,
                                                const uint32_t expected_uid) {
    DIR *dir = opendir("/sys/class/input/");
    if (!dir) throw std::runtime_error("Can't access input devices");

    std::string found_path;
    dirent *entry;

    while ((entry = readdir(dir))) {
        std::string name(entry->d_name);
        if (name.substr(0, 5) != "event") continue;

        try {
            std::string sysfs_path = "/sys/class/input/" + name + "/device/";
            std::string dev_path = "/dev/input/" + name;

            uint16_t vendor = read_id_from_file(sysfs_path + "id/vendor");
            uint16_t product = read_id_from_file(sysfs_path + "id/product");
            if (vendor != vendor_id || product != product_id) continue;

            int tmp_fd = open(dev_path.c_str(), O_RDONLY);
            if (tmp_fd < 0) continue;

            DeviceCapabilities caps = get_device_capabilities(tmp_fd);
            close(tmp_fd);

            if (generate_uid(caps) == expected_uid) {
                found_path = dev_path;
                break;
            }
        } catch (...) {
            /* Skip problematic devices */
        }
    }

    closedir(dir);
    if (found_path.empty()) throw std::runtime_error("Device not found");
    return found_path;
}

void VirtualInputProxy::create_virtual_device() {
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

void VirtualInputProxy::setup_capabilities() const {
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

void VirtualInputProxy::setup_event_codes(const int ev_type) const {
    unsigned long codes[KEY_MAX / 8 + 1] = {};
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

int VirtualInputProxy::get_max_code(const int ev_type) {
    switch (ev_type) {
        case EV_KEY: return KEY_MAX;
        case EV_REL: return REL_MAX;
        case EV_ABS: return ABS_MAX;
        case EV_MSC: return MSC_MAX;
        case EV_LED: return LED_MAX;
        default: return 0;
    }
}

void VirtualInputProxy::set_virtual_bit(const int ev_type, const int code) const {
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

void VirtualInputProxy::handle_event(const input_event &ev) const {
    if (ev.type == EV_KEY && ev.code == target_key_) {
        if (callback_) callback_(ev.value);
    } else {
        write(ufd_, &ev, sizeof(ev));
    }
}
