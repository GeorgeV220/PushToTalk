#ifndef DEVICECAPABILITIES_H
#define DEVICECAPABILITIES_H

#include <bitset>
#include <cstdint>
#include <string>
#include <linux/input-event-codes.h>
#include <bitset>
#include <linux/input.h>
#include <unordered_map>

struct DeviceCapabilities {
    std::string name;

    std::bitset<KEY_MAX + 1> key_bits;
    std::bitset<ABS_MAX + 1> abs_bits;
    std::bitset<REL_MAX + 1> rel_bits;

    std::unordered_map<int, input_absinfo> abs_info;

    int num_keys = 0;
};

namespace DeviceUtils {
    uint16_t read_id_from_file(const std::string &path);

    DeviceCapabilities get_device_capabilities(int fd);

    uint32_t generate_uid(const DeviceCapabilities &caps);

    bool test_bit(int bit, const unsigned long *arr);
}

#endif // DEVICECAPABILITIES_H
