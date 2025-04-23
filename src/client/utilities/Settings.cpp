#include "Settings.h"
#include "../../common/utilities/Utility.h"
#include <fstream>
#include <sys/stat.h>
#include <linux/input.h>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <pwd.h>

#include "../../common/utilities/numbers/Conversion.h"

#define DEFAULT_PRODUCT_ID 0x0
#define DEFAULT_VENDOR_ID 0x0
#define DEFAULT_UID 0x0
#define DEFAULT_BUTTON BTN_EXTRA
#define PTT_ON_PATH "/home/<user>/Music/ptt_on.mp3"
#define PTT_OFF_PATH "/home/<user>/Music/ptt_off.mp3"
#define DEFAULT_VOLUME 0.1f

// static
Settings Settings::settings;

Settings::Settings() : sButton(DEFAULT_BUTTON),
                       sPttOnPath(PTT_ON_PATH), sPttOffPath(PTT_OFF_PATH),
                       sVolume(DEFAULT_VOLUME) {
    const char *homeDir = nullptr;

    if (const char *sudoUser = getenv("SUDO_USER")) {
        if (const passwd *pw = getpwnam(sudoUser)) {
            homeDir = pw->pw_dir;
        }
    }

    if (!homeDir) {
        homeDir = getenv("HOME");
    }

    if (!homeDir) {
        Utility::error("Unable to determine the home directory");
        return;
    }

    configDirPath = std::string(homeDir) + "/.config";
    configFilePath = configDirPath + "/ptt.properties";
    Utility::print(configFilePath);
}

void Settings::saveSettings(
    const std::string &device,
    int button,
    std::string pttOnPath,
    std::string pttOffPath,
    float volume
) {
    std::lock_guard lock(settingsMutex);
    sDevice = device;
    sButton = button;
    sPttOnPath = std::move(pttOnPath);
    sPttOffPath = std::move(pttOffPath);
    sVolume = volume;

    // Create config directory if needed
    struct stat st{};
    if (stat(configDirPath.c_str(), &st) == -1 && mkdir(configDirPath.c_str(), 0755) == -1) {
        Utility::error("Could not create config directory");
        return;
    }

    std::ofstream file(configFilePath);
    if (file.is_open()) {
        file << "device = " << device << "\n";
        file << "button = " << sButton << "\n";
        file << "pttonpath = " << sPttOnPath << "\n";
        file << "pttoffpath = " << sPttOffPath << "\n";
        file << "volume = " << std::fixed << sVolume << "\n";
        Utility::debugPrint("Saving settings");
    } else {
        Utility::error("Could not open settings file for writing");
    }
}

void Settings::loadSettings() {
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        Utility::debugPrint("No settings file found, using default settings.");
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        auto [key, value] = Utility::splitKeyValue(line);
        if (key.empty()) continue;

        if (key == "device") {
            sDevice = value;
        } else if (key == "button") {
            if (const IntConversionResult result = safeStrToInt(value); result.success) {
                sButton = result.value;
            } else {
                Utility::error("Failed to parse button value");
            }
        } else if (key == "pttonpath") {
            sPttOnPath = value;
        } else if (key == "pttoffpath") {
            sPttOffPath = value;
        } else if (key == "volume") {
            if (const FloatConversionResult result = safeStrToFloat(value); result.success) {
                sVolume = result.value;
            } else {
                Utility::error("Failed to parse volume value");
            }
        }
    }
}

uint16_t Settings::getVendorID() const {
    uint16_t vendor = DEFAULT_VENDOR_ID;
    if (const auto parts = Utility::split(sDevice, ':'); parts.size() == 3) {
        const std::string &strVendor = parts[0];

        vendor = safeStrToUInt16(strVendor).success ? safeStrToUInt16(strVendor).value : DEFAULT_VENDOR_ID;
    }
    return vendor;
}

uint16_t Settings::getProductID() const {
    uint16_t product = DEFAULT_PRODUCT_ID;
    if (const auto parts = Utility::split(sDevice, ':'); parts.size() == 3) {
        const std::string &strProduct = parts[1];
        product = safeStrToUInt16(strProduct).success ? safeStrToUInt16(strProduct).value : DEFAULT_PRODUCT_ID;
    }
    return product;
}

uint32_t Settings::getDeviceUID() const {
    uint32_t uid = DEFAULT_UID;
    if (const auto parts = Utility::split(sDevice, ':'); parts.size() == 3) {
        const std::string &strUID = parts[2];

        uid = safeStrToUInt32(strUID).success ? safeStrToUInt32(strUID).value : DEFAULT_UID;
    }
    return uid;
}
