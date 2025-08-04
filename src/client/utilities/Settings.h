#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <vector>
#include <mutex>

#include "common/utilities/Utility.h"
#include "common/utilities/numbers/Conversion.h"

struct DeviceSettings {
    std::string deviceStr;
    int button;

    [[nodiscard]] uint32_t getVendorID() const {
        const auto result = safeStrToUInt32(Utility::split(deviceStr, ':')[0]);
        return result.success ? result.value : 0;
    }

    [[nodiscard]] uint32_t getProductID() const {
        const auto result = safeStrToUInt32(Utility::split(deviceStr, ':')[1]);
        return result.success ? result.value : 0;
    }

    [[nodiscard]] uint32_t getDeviceUID() const {
        const auto result = safeStrToUInt32(Utility::split(deviceStr, ':')[2]);
        return result.success ? result.value : 0;
    }
};

class Settings {
public:
    static Settings settings;

    std::vector<DeviceSettings> devices;
    std::string sPttOnPath;
    std::string sPttOffPath;
    float sVolume;

    void saveSettings();

    void loadSettings();

    std::string configFilePath;
    std::string configDirPath;

private:
    std::mutex settingsMutex;

    Settings();
};

#endif // SETTINGS_H
