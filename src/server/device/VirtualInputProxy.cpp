#include "VirtualInputProxy.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <linux/uinput.h>

#include "common/utilities/Utility.h"
#include "common/device/DeviceCapabilities.h"

using namespace DeviceUtils;

VirtualInputProxy::VirtualInputProxy(const std::vector<DeviceConfig> &configs) {
    for (const auto &[vendor_id, product_id, uid, target_key]: configs) {
        std::string device_path = find_device_path(vendor_id, product_id, uid);
        const int fd_physical = open(device_path.c_str(), O_RDWR);
        if (fd_physical < 0) {
            throw std::runtime_error("Failed to open input device: " + device_path);
        }

        if (ioctl(fd_physical, EVIOCGRAB, 1) < 0) {
            close(fd_physical);
            throw std::runtime_error("Failed to grab physical device: " + device_path);
        }

        const int ufd = create_virtual_device(fd_physical);

        DeviceContext ctx;
        ctx.fd_physical = fd_physical;
        ctx.ufd = ufd;
        ctx.target_key = target_key;
        ctx.running = false;

        contexts_.push_back(std::move(ctx));
    }
}

VirtualInputProxy::~VirtualInputProxy() {
    stop();
    for (const auto &ctx: contexts_) {
        if (ctx.ufd >= 0) {
            ioctl(ctx.ufd, UI_DEV_DESTROY);
            close(ctx.ufd);
        }
        if (ctx.fd_physical >= 0) {
            ioctl(ctx.fd_physical, EVIOCGRAB, 0);
            close(ctx.fd_physical);
        }
    }
}

void VirtualInputProxy::set_callback(Callback callback) {
    callback_ = std::move(callback);
}

void VirtualInputProxy::start() {
    for (auto &ctx: contexts_) {
        if (ctx.running) continue;

        ctx.running = true;
        ctx.listener_thread = std::thread([this, &ctx]() {
            input_event ev{};
            while (ctx.running) {
                if (const ssize_t bytes = Utility::safe_read(ctx.fd_physical, &ev, sizeof(ev)); bytes == sizeof(ev)) {
                    handle_event(ctx, ev);
                } else if (bytes < 0) {
                    if (errno != EAGAIN && errno != EINTR) break;
                }
            }
        });
    }
}

void VirtualInputProxy::stop() {
    for (auto &ctx: contexts_) {
        ctx.running = false;
    }
    for (auto &ctx: contexts_) {
        if (ctx.listener_thread.joinable()) {
            ctx.listener_thread.join();
        }
    }
}

std::string VirtualInputProxy::find_device_path(const uint16_t vendor_id, const uint16_t product_id,
                                                const uint32_t expected_uid) {
    std::cout << std::hex << vendor_id << ":" << std::hex << product_id << ":" << std::hex << expected_uid << std::endl;
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

            const uint16_t vendor = read_id_from_file(sysfs_path + "id/vendor");
            // ReSharper disable once CppTooWideScopeInitStatement
            const uint16_t product = read_id_from_file(sysfs_path + "id/product");
            if (vendor != vendor_id || product != product_id) continue;

            const int tmp_fd = open(dev_path.c_str(), O_RDONLY);
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

int VirtualInputProxy::create_virtual_device(const int physical_fd) {
    const int ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) {
        throw std::runtime_error("Failed to open uinput");
    }

    setup_capabilities(physical_fd, ufd);

    uinput_user_dev uidev = {};
    strncpy(uidev.name, "PTT Virtual Device", UINPUT_MAX_NAME_SIZE);
    uidev.id.bustype = BUS_VIRTUAL;

    if (Utility::safe_write(ufd, &uidev, sizeof(uidev)) != sizeof(uidev)) {
        close(ufd);
        throw std::runtime_error("Failed to write uinput config");
    }

    if (ioctl(ufd, UI_DEV_CREATE) < 0) {
        close(ufd);
        throw std::runtime_error("Failed to create virtual device");
    }

    return ufd;
}

void VirtualInputProxy::setup_capabilities(const int physical_fd, const int ufd) {
    unsigned long evtypes[EV_MAX / 8 + 1] = {0};
    if (ioctl(physical_fd, EVIOCGBIT(0, EV_MAX), evtypes) < 0) {
        close(ufd);
        throw std::runtime_error("Failed to get event types");
    }

    for (int ev = 0; ev < EV_MAX; ++ev) {
        if (test_bit(ev, evtypes)) {
            ioctl(ufd, UI_SET_EVBIT, ev);
            setup_event_codes(physical_fd, ufd, ev);
        }
    }
}

void VirtualInputProxy::setup_event_codes(const int physical_fd, const int ufd, const int ev_type) {
    unsigned long codes[KEY_MAX / 8 + 1] = {};
    const int max_code = get_max_code(ev_type);

    if (ioctl(physical_fd, EVIOCGBIT(ev_type, max_code), codes) < 0) {
        return;
    }

    for (int code = 0; code < max_code; ++code) {
        if (test_bit(code, codes)) {
            set_virtual_bit(ufd, ev_type, code);
        }
    }
}

int VirtualInputProxy::get_max_code(int ev_type) {
    switch (ev_type) {
        case EV_KEY: return KEY_MAX;
        case EV_REL: return REL_MAX;
        case EV_ABS: return ABS_MAX;
        case EV_MSC: return MSC_MAX;
        case EV_LED: return LED_MAX;
        default: return 0;
    }
}

void VirtualInputProxy::set_virtual_bit(const int ufd, const int ev_type, const int code) {
    switch (ev_type) {
        case EV_KEY: ioctl(ufd, UI_SET_KEYBIT, code);
            break;
        case EV_REL: ioctl(ufd, UI_SET_RELBIT, code);
            break;
        case EV_ABS: ioctl(ufd, UI_SET_ABSBIT, code);
            break;
        case EV_MSC: ioctl(ufd, UI_SET_MSCBIT, code);
            break;
        case EV_LED: ioctl(ufd, UI_SET_LEDBIT, code);
            break;
        default: break;
    }
}

void VirtualInputProxy::handle_event(DeviceContext &ctx, const input_event &ev) const {
    if (ev.type == EV_KEY && ev.code == ctx.target_key) {
        if (callback_) callback_(ctx.target_key, ev.value);
    } else {
        Utility::safe_write(ctx.ufd, &ev, sizeof(ev));
    }
}


void VirtualInputProxy::detect_devices() {
    Utility::debugPrint("Device detection mode - press keys to see their device info (Ctrl+C to exit)\n");

    std::vector<int> fds;
    std::vector<std::string> device_paths;

    DIR *dir = opendir("/dev/input/");
    if (!dir) {
        Utility::error("Error accessing input devices (try running as root)");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        std::string name(entry->d_name);
        if (name.substr(0, 5) == "event") {
            std::string path = "/dev/input/" + name;

            int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                fds.push_back(fd);
                device_paths.push_back(path);
            }
        }
    }
    closedir(dir);

    while (true) {
        fd_set set;
        FD_ZERO(&set);
        int max_fd = 0;

        for (int fd: fds) {
            FD_SET(fd, &set);
            if (fd > max_fd) max_fd = fd;
        }

        timeval timeout{0, 100000};
        if (select(max_fd + 1, &set, nullptr, nullptr, &timeout) < 0) break;

        for (size_t i = 0; i < fds.size(); i++) {
            if (FD_ISSET(fds[i], &set)) {
                input_event ev{};
                while (Utility::safe_read(fds[i], &ev, sizeof(ev)) == sizeof(ev)) {
                    if (ev.type == EV_KEY && ev.value == 1) {
                        DeviceCapabilities caps = get_device_capabilities(fds[i]);
                        uint16_t vendor = 0, product = 0;

                        try {
                            std::string sysfs_path = "/sys/class/input/" +
                                                     device_paths[i].substr(strlen("/dev/input/")) + "/device/id/";
                            vendor = read_id_from_file(sysfs_path + "vendor");
                            product = read_id_from_file(sysfs_path + "product");
                        } catch (...) {
                        }
                        std::ostringstream oss;
                        oss << "Key pressed: 0x" << std::hex << ev.code << "\n"
                                << "Device: " << device_paths[i] << "\n"
                                << "Vendor: 0x" << std::setw(4) << std::setfill('0') << vendor << "\n"
                                << "Product: 0x" << std::setw(4) << std::setfill('0') << product << "\n"
                                << "UID: 0x" << std::setw(8) << std::setfill('0') << DeviceUtils::generate_uid(caps) <<
                                "\n"
                                << "Name: " << caps.name << "\n\n";
                        Utility::print(oss.str());

                        std::ostringstream hex_oss;
                        hex_oss << std::hex << std::setfill('0');

                        hex_oss << std::setw(4) << vendor;
                        const std::string vendor_str = "0x" + hex_oss.str();
                        hex_oss.str("");

                        hex_oss << std::setw(4) << product;
                        const std::string product_str = "0x" + hex_oss.str();
                        hex_oss.str("");

                        hex_oss << std::setw(8) << DeviceUtils::generate_uid(caps);
                        const std::string uid_str = "0x" + hex_oss.str();

                        std::string device;
                        device.append(vendor_str)
                                .append(":")
                                .append(product_str)
                                .append(":")
                                .append(uid_str);
                        Utility::print("Device to use in the config: " + device);
                    }
                }
            }
        }
    }

    for (int fd: fds) close(fd);
}
