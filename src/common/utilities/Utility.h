#ifndef UTILITY_H
#define UTILITY_H

#include <cstdint>
#include <string>
#include <vector>


struct DeviceConfig {
    uint16_t vendor_id;
    uint16_t product_id;
    uint32_t uid;
    int target_key;
};

struct InitParams {
    std::vector<DeviceConfig> configs;
};

struct UserInfo {
    std::string name;
    std::string path;
    uid_t uid;
    gid_t gid;
};

class Utility {
public:
    // Function declarations
    static void set_debug(bool enabled);

    static bool is_debug_enabled();

    static std::string trim(const std::string &str);

    static std::pair<std::string, std::string> splitKeyValue(const std::string &line);

    static std::vector<std::string> split(const std::string &s, char delimiter);

    static void print(const std::string &message);

    static void error(const std::string &message);

    static void debugPrint(const std::string &message);

    static void pError(const std::string &message);

    static UserInfo get_active_user_info();

    static ssize_t safe_read(int fd, void *buffer, size_t size);

    static ssize_t safe_write(int fd, const void *buffer, size_t size);

private:
    Utility() = delete;

    // Internal static state
    static inline bool debug_enabled_ = false;
};

#endif // UTILITY_H
