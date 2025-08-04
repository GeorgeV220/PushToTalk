#include "DeviceCapabilities.h"
#include <fstream>
#include <sstream>
#include <zlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdexcept>
#include <linux/input.h>

using namespace DeviceUtils;

uint16_t DeviceUtils::read_id_from_file(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Can't read ID from " + path);

    std::string id_str;
    file >> id_str;
    return static_cast<uint16_t>(std::stoul(id_str, nullptr, 16));
}

bool DeviceUtils::test_bit(const int bit, const unsigned long *arr) {
    return arr[bit / (sizeof(long) * 8)] & (1UL << (bit % (sizeof(long) * 8)));
}

DeviceCapabilities DeviceUtils::get_device_capabilities(const int fd) {
    DeviceCapabilities caps;

    char name[256] = "Unknown";
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
        caps.name = name;
    }

    unsigned long key_bits_raw[KEY_MAX / (sizeof(long) * 8) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits_raw)), key_bits_raw) >= 0) {
        for (int i = 0; i < KEY_MAX; i++) {
            if (test_bit(i, key_bits_raw)) {
                caps.key_bits.set(i);
                caps.num_keys++;
            }
        }
    }

    if (unsigned long abs_bits[ABS_MAX / 8 + 1] = {}; ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) >= 0) {
        for (int i = 0; i < ABS_MAX; i++) {
            if (test_bit(i, abs_bits)) caps.abs_mask |= (1 << i);
        }
    }

    unsigned long rel_bits[REL_MAX / 8 + 1] = {0};
    if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), rel_bits) >= 0) {
        for (int i = 0; i < REL_MAX; i++) {
            if (test_bit(i, rel_bits)) caps.rel_mask |= (1 << i);
        }
    }

    return caps;
}

uint32_t DeviceUtils::generate_uid(const DeviceCapabilities &caps) {
    std::stringstream ss;

    ss << caps.name << ":" << caps.num_keys << ":" << caps.abs_mask << ":" << caps.rel_mask << ":";

    for (int i = 0; i < KEY_MAX; ++i) {
        if (caps.key_bits.test(i)) ss << "K" << i << ",";
    }

    for (int i = 0; i < ABS_MAX; ++i) {
        if ((caps.abs_mask >> i) & 1) ss << "A" << i << ",";
    }

    for (int i = 0; i < REL_MAX; ++i) {
        if ((caps.rel_mask >> i) & 1) ss << "R" << i << ",";
    }

    const std::string uid_str = ss.str();
    return crc32(0, reinterpret_cast<const Bytef *>(uid_str.data()), uid_str.size());
}
