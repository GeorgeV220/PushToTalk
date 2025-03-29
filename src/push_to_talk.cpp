#include "push_to_talk.h"

#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iomanip>
#include <sstream>

#include "Settings.h"
#include "device/VirtualInputProxy.h"
#include "Utility.h"
#include "utilities/numbers/Conversion.h"

PushToTalk::PushToTalk()
    : proxy(Settings::settings.getVendorID(), Settings::settings.getProductID(), Settings::settings.getDeviceUID(),
            Settings::settings.sButton) {
    Utility::initAudioSystem();
}

PushToTalk::~PushToTalk() { Utility::cleanupAudioSystem(); }

int PushToTalk::run() {
    try {
        proxy.set_callback([](const bool pressed) {
            Utility::debugPrint(
                "Button " + safeIntToStr(Settings::settings.sButton).str + " " + std::string(
                    pressed ? "pressed" : "released"));
            Utility::playSound((!pressed ? Settings::settings.sPttOffPath : Settings::settings.sPttOnPath).c_str());
            Utility::setMicMute(!pressed);
        });

        proxy.start();
        while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (const std::exception &e) {
        Utility::error("Error: " + std::string(e.what()));
        return 1;
    }
}

void PushToTalk::detect_devices() {
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

        timeval timeout{0, 100000}; // 100ms timeout
        if (select(max_fd + 1, &set, nullptr, nullptr, &timeout) < 0) break;

        for (size_t i = 0; i < fds.size(); i++) {
            if (FD_ISSET(fds[i], &set)) {
                input_event ev{};
                while (read(fds[i], &ev, sizeof(ev)) == sizeof(ev)) {
                    if (ev.type == EV_KEY && ev.value == 1) {
                        DeviceCapabilities caps = DeviceUtils::get_device_capabilities(fds[i]);
                        uint16_t vendor = 0, product = 0;

                        try {
                            std::string sysfs_path = "/sys/class/input/" +
                                                     device_paths[i].substr(strlen("/dev/input/")) + "/device/id/";
                            vendor = DeviceUtils::read_id_from_file(sysfs_path + "vendor");
                            product = DeviceUtils::read_id_from_file(sysfs_path + "product");
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

                        // Vendor (16-bit)
                        hex_oss << std::setw(4) << vendor;
                        const std::string vendor_str = "0x" + hex_oss.str();
                        hex_oss.str("");

                        // Product (16-bit)
                        hex_oss << std::setw(4) << product;
                        const std::string product_str = "0x" + hex_oss.str();
                        hex_oss.str("");

                        // UID (32-bit)
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
