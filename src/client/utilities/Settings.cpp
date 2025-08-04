#include "Settings.h"
#include "common/utilities/Utility.h"
#include "common/utilities/numbers/Conversion.h"

#include <fstream>
#include <sys/stat.h>
#include <cstdlib>
#include <map>

#define DEFAULT_VOLUME 0.1f
#define DEFAULT_PATH "/home/<user>/Music/ptt_on.mp3"

Settings Settings::settings;

Settings::Settings() : sPttOnPath(DEFAULT_PATH), sPttOffPath(DEFAULT_PATH), sVolume(DEFAULT_VOLUME) {
    const char *homeDir = getenv("HOME");
    if (!homeDir) {
        Utility::error("Unable to determine the home directory");
        return;
    }
    configDirPath = std::string(homeDir) + "/.config";
    configFilePath = configDirPath + "/ptt.properties";
}

void Settings::saveSettings() {
    std::lock_guard lock(settingsMutex);
    struct stat st{};
    if (stat(configDirPath.c_str(), &st) == -1 && mkdir(configDirPath.c_str(), 0755) == -1) {
        Utility::error("Could not create config directory");
        return;
    }

    std::ofstream file(configFilePath);
    if (!file.is_open()) {
        Utility::error("Could not open settings file for writing");
        return;
    }

    for (size_t i = 0; i < devices.size(); ++i) {
        file << "device" << i << " = " << devices[i].deviceStr << "\n";
        file << "button" << i << " = " << devices[i].button << "\n";
    }
    file << "pttonpath = " << sPttOnPath << "\n";
    file << "pttoffpath = " << sPttOffPath << "\n";
    file << "volume = " << std::fixed << sVolume << "\n";
}

void Settings::loadSettings() {
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        Utility::debugPrint("No settings file found, using default settings.");
        return;
    }

    devices.clear();
    std::string line;
    std::map<int, DeviceSettings> tempDevices;

    while (std::getline(file, line)) {
        auto [key, value] = Utility::splitKeyValue(line);
        if (key.empty()) continue;

        if (key.starts_with("device")) {
            auto indexResult = safeStrToInt(key.substr(6));
            if (indexResult.success && indexResult.value >= 0) {
                tempDevices[indexResult.value].deviceStr = value;
            }
        } else if (key.starts_with("button")) {
            auto indexResult = safeStrToInt(key.substr(6));
            auto valueResult = safeStrToInt(value);
            if (indexResult.success && valueResult.success && indexResult.value >= 0) {
                tempDevices[indexResult.value].button = valueResult.value;
            }
        } else if (key == "pttonpath") {
            sPttOnPath = value;
        } else if (key == "pttoffpath") {
            sPttOffPath = value;
        } else if (key == "volume") {
            auto result = safeStrToFloat(value);
            if (result.success) {
                sVolume = result.value;
            }
        }
    }

    for (const auto &[_, dev]: tempDevices) {
        devices.push_back(dev);
    }
}
