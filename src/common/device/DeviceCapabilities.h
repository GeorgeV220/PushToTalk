#ifndef DEVICECAPABILITIES_H
#define DEVICECAPABILITIES_H

#include <cstdint>
#include <string>

struct DeviceCapabilities {
    uint32_t num_keys = 0;
    uint32_t abs_mask = 0;
    uint32_t rel_mask = 0;
    std::string name;
};

namespace DeviceUtils {
    uint16_t read_id_from_file(const std::string &path);

    DeviceCapabilities get_device_capabilities(int fd);

    uint32_t generate_uid(const DeviceCapabilities &caps);

    bool test_bit(int bit, const unsigned long *arr);
}

#endif // DEVICECAPABILITIES_H
