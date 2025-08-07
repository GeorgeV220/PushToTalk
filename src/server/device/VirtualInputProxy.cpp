#include "VirtualInputProxy.h"

#include <algorithm>
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

static uint16_t vendor_counter = 0;
static uint16_t product_counter = 0;

void VirtualInputProxy::add_failed_config(const DeviceConfig &config) {
    const auto already_failed = std::ranges::any_of(failed_configs,
                                                    [&](const DeviceConfig &dc) {
                                                        return dc.vendor_id == config.vendor_id &&
                                                               dc.product_id == config.product_id &&
                                                               dc.uid == config.uid &&
                                                               dc.target_key == config.target_key;
                                                    });
    if (!already_failed) {
        failed_configs.push_back(config);
    }
}

void VirtualInputProxy::remove_failed_config(const DeviceConfig &config) {
    std::erase_if(failed_configs,
                  [&](const DeviceConfig &dc) {
                      return dc.vendor_id == config.vendor_id &&
                             dc.product_id == config.product_id &&
                             dc.uid == config.uid &&
                             dc.target_key == config.target_key;
                  });
}

void VirtualInputProxy::retry_failed_configs() {
    for (const auto configs_copy = failed_configs; const auto &config: configs_copy) {
        remove_failed_config(config);
        add_device(config);
    }
}


void VirtualInputProxy::add_device(const DeviceConfig &config) {
    const auto &[vendor_id, product_id, uid, target_key] = config;
    const std::string device_path = find_device_path(vendor_id, product_id, uid);
    if (device_path.empty()) {
        add_failed_config(config);

        Utility::error(
                "Failed to find input device: " + std::to_string(vendor_id) + ":" + std::to_string(product_id) + ":" +
                std::to_string(uid)
        );
        return;
    }

    const int fd_physical = open(device_path.c_str(), O_RDWR);
    if (fd_physical < 0) {
        add_failed_config(config);

        Utility::error("Failed to open input device: " + device_path);
        return;
    }

    if (ioctl(fd_physical, EVIOCGRAB, 1) < 0) {
        add_failed_config(config);

        close(fd_physical);
        Utility::error("Failed to grab physical device: " + device_path);
        return;
    }

    const int ufd = create_virtual_device(fd_physical);
    if (ufd < 0) {
        add_failed_config(config);

        ioctl(fd_physical, EVIOCGRAB, 0);
        close(fd_physical);
        Utility::error("Failed to create virtual device for: " + device_path);
        return;
    }

    remove_failed_config(config);

    DeviceContext ctx;
    ctx.fd_physical = fd_physical;
    ctx.ufd = ufd;
    ctx.target_key = target_key;
    ctx.running = false;

    contexts_.push_back(std::move(ctx));
}

void VirtualInputProxy::remove_device(const DeviceConfig &config) {
    const auto it = std::ranges::find_if(contexts_,
                                         [&](const DeviceContext &ctx) {
                                             return ctx.target_key == config.target_key;
                                         });

    if (it != contexts_.end()) {
        it->running = false;
        if (it->listener_thread.joinable()) {
            it->listener_thread.join();
        }
        if (it->fd_physical >= 0) {
            ioctl(it->fd_physical, EVIOCGRAB, 0);
            close(it->fd_physical);
        }
        if (it->ufd >= 0) {
            ioctl(it->ufd, UI_DEV_DESTROY);
            close(it->ufd);
        }
        contexts_.erase(it);
    }

    remove_failed_config(config);
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

void VirtualInputProxy::start_retry_loop() {
    if (running) return;
    running = true;
    retry_thread = std::thread([this]() {
        while (running) {
            retry_failed_configs();
            for (int i = 0; i < 50 && running; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
}


void VirtualInputProxy::stop_retry_loop() {
    running = false;
    if (retry_thread.joinable())
        retry_thread.join();
}

void VirtualInputProxy::start() {
    start_retry_loop();
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
    stop_retry_loop();
}

std::string VirtualInputProxy::find_device_path(const uint16_t vendor_id, const uint16_t product_id,
                                                const uint32_t expected_uid) {
    std::cout << std::hex << vendor_id << ":" << std::hex << product_id << ":" << std::hex << expected_uid << std::endl;
    DIR *dir = opendir("/sys/class/input/");
    if (!dir) {
        Utility::error("Can't open input devices directory");
        return "";
    }

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
    if (found_path.empty()) {
        Utility::error("Device not found");
        return "";
    }
    return found_path;
}

int VirtualInputProxy::create_virtual_device(const int physical_fd) {
    const int ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) {
        Utility::error("Failed to open /dev/uinput");
        return -1;
    }

    bool has_ff = false;
    try {
        has_ff = setup_capabilities(physical_fd, ufd);
    } catch (const std::exception &e) {
        std::cerr << "setup_capabilities failed: " << e.what() << std::endl;
        close(ufd);
        return -1;
    }

    uinput_user_dev uidev = {};
    const std::string dev_name = "PTT Virtual Device " + std::to_string(ufd);
    strncpy(uidev.name, dev_name.c_str(), UINPUT_MAX_NAME_SIZE - 1);
    uidev.name[UINPUT_MAX_NAME_SIZE - 1] = '\0';

    uidev.id.bustype = BUS_USB;

    uidev.id.vendor = 0x1234 + (vendor_counter++);
    uidev.id.product = 0x5678 + (product_counter++);
    uidev.id.version = 1;

    if (has_ff) {
        uidev.ff_effects_max = 16;
    }

    if (Utility::safe_write(ufd, &uidev, sizeof(uidev)) != sizeof(uidev)) {
        Utility::error("Failed to write uinput_user_dev struct");
        close(ufd);
        return -1;
    }

    if (ioctl(ufd, UI_DEV_CREATE) < 0) {
        Utility::error("Failed to create virtual uinput device");
        close(ufd);
        return -1;
    }

    std::cout << "Successfully created virtual device: " << dev_name << std::endl;
    return ufd;
}

bool VirtualInputProxy::setup_capabilities(const int physical_fd, const int ufd) {
    unsigned long evtypes[EV_MAX / (sizeof(long) * 8) + 1] = {0};
    if (ioctl(physical_fd, EVIOCGBIT(0, sizeof(evtypes)), evtypes) < 0) {
        close(ufd);
        Utility::error("Failed to get event types from physical_fd: " + std::string(strerror(errno)));
        throw std::runtime_error("Failed to get event types: " + std::string(strerror(errno)));
    }

    bool has_ff = false;

    for (int ev = 0; ev < EV_MAX; ++ev) {
        if (test_bit(ev, evtypes)) {
            Utility::debugPrint("Setting event type: " + std::to_string(ev) + "\n");
            if (ioctl(ufd, UI_SET_EVBIT, ev) < 0) {
                close(ufd);
                Utility::error("UI_SET_EVBIT failed for event type " + std::to_string(ev) + ": " +
                               std::string(strerror(errno)));
                throw std::runtime_error(
                        "UI_SET_EVBIT failed for event type " + std::to_string(ev) + ": " +
                        std::string(strerror(errno)));
            }
            if (ev == EV_FF) {
                has_ff = true;
            }
            setup_event_codes(physical_fd, ufd, ev);
        }
    }

    return has_ff;
}

void VirtualInputProxy::setup_event_codes(const int physical_fd, const int ufd, const int ev_type) {
    const int max_code = get_max_code(ev_type);
    unsigned long codes[(max_code + sizeof(long) * 8 - 1) / (sizeof(long) * 8)] = {0};

    if (ioctl(physical_fd, EVIOCGBIT(ev_type, sizeof(codes)), codes) < 0) {
        throw std::runtime_error(
                "Failed to get event codes for ev_type " + std::to_string(ev_type) + ": " +
                std::string(strerror(errno)));
    }

    for (int code = 0; code < max_code; ++code) {
        if (test_bit(code, codes)) {
            set_virtual_bit(ufd, ev_type, code, physical_fd);
        }
    }
}

int VirtualInputProxy::get_max_code(const int ev_type) {
    switch (ev_type) {
        case EV_KEY:
            return KEY_MAX;
        case EV_REL:
            return REL_MAX;
        case EV_ABS:
            return ABS_MAX;
        case EV_MSC:
            return MSC_MAX;
        case EV_LED:
            return LED_MAX;
        default:
            return 0;
    }
}

void VirtualInputProxy::set_virtual_bit(const int ufd, const int ev_type, const int code, const int physical_fd) {
    switch (ev_type) {
        case EV_KEY:
            ioctl(ufd, UI_SET_KEYBIT, code);
            break;
        case EV_REL:
            ioctl(ufd, UI_SET_RELBIT, code);
            break;
        case EV_ABS: {
            if (ioctl(ufd, UI_SET_ABSBIT, code) < 0) {
                Utility::error("Failed to set ABS bit for code: " + std::to_string(code));
                break;
            }

            input_absinfo absinfo{};
            if (ioctl(physical_fd, EVIOCGABS(code), &absinfo) == 0) {
                struct uinput_abs_setup abs = {};
                abs.code = code;
                abs.absinfo = absinfo;

                Utility::error("ABS[" + std::to_string(code) + "] - min: " + std::to_string(absinfo.minimum) +
                               ", max: " + std::to_string(absinfo.maximum) +
                               ", flat: " + std::to_string(absinfo.flat) +
                               ", fuzz: " + std::to_string(absinfo.fuzz) +
                               ", resolution: " + std::to_string(absinfo.resolution));

                if (ioctl(ufd, UI_ABS_SETUP, &abs) < 0) {
                    Utility::error("Failed UI_ABS_SETUP for ABS[" + std::to_string(code) + "]: " +
                                   std::string(strerror(errno)));
                }
            } else {
                Utility::error("Failed fallback EVIOCGABS for ABS[" + std::to_string(code) + "]: " +
                               std::string(strerror(errno)));

                uinput_abs_setup abs = {};
                abs.code = code;
                abs.absinfo.minimum = 0;
                abs.absinfo.maximum = 255;
                abs.absinfo.flat = 0;
                abs.absinfo.fuzz = 0;
                abs.absinfo.resolution = 0;

                if (ioctl(ufd, UI_ABS_SETUP, &abs) < 0) {
                    Utility::error("Failed fallback UI_ABS_SETUP for ABS[" + std::to_string(code) + "]: " +
                                   std::string(strerror(errno)));
                }
            }
            break;
        }
        case EV_MSC:
            ioctl(ufd, UI_SET_MSCBIT, code);
            break;
        case EV_LED:
            ioctl(ufd, UI_SET_LEDBIT, code);
            break;
        default:
            break;
    }
}

void VirtualInputProxy::handle_event(const DeviceContext &ctx, const input_event &ev) const {
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
        if (std::string name(entry->d_name); name.substr(0, 5) == "event") {
            std::string path = "/dev/input/" + name;

            if (int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK); fd >= 0) {
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
                    if (ev.type == EV_KEY || ev.type == EV_ABS && ev.value == 1) {
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
                            << "Name: " << caps.name << "\n";
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
                        Utility::print("Device to use in the config: " + device + "\n\n");
                    }
                }
            }
        }
    }

    for (int fd: fds) close(fd);
}
