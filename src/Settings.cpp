#include "Settings.h"
#include "Utility.h"
#include <fstream>
#include <sys/stat.h>
#include <linux/input.h>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <pwd.h>

#define DEFAULT_DEVICE "/dev/input/event5"
#define DEFAULT_BUTTON BTN_EXTRA
#define DEFAULT_BUTTON2 BTN_SIDE
#define PTT_ON_PATH "/home/<user>/Music/ptt_on.mp3"
#define PTT_OFF_PATH "/home/<user>/Music/ptt_off.mp3"
#define DEFAULT_VOLUME 0.1f

Settings::Settings() : sDevice(DEFAULT_DEVICE), sButton(DEFAULT_BUTTON),
                       sButton2(DEFAULT_BUTTON2), sPttOnPath(PTT_ON_PATH),
                       sPttOffPath(PTT_OFF_PATH), sVolume(DEFAULT_VOLUME) {
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


ConversionResult Settings::safeAtoi(const std::string &str) {
    ConversionResult result;
    try {
        size_t idx;
        const int value = std::stoi(str, &idx);

        if (idx != str.length()) {
            throw std::invalid_argument("Trailing characters");
        }

        result.value = value;
        result.success = true;
    } catch ([[maybe_unused]] const std::exception &e) {
        result.success = false;
    }
    return result;
}

void Settings::saveSettings(
    const std::string &device,
    int button1,
    int button2,
    std::string pttOnPath,
    std::string pttOffPath,
    float volume
) {
    sDevice = device;
    sButton = button1;
    sButton2 = button2;
    sPttOnPath = std::move(pttOnPath);
    sPttOffPath = std::move(pttOffPath);

    struct stat st{};
    if (stat(configDirPath.c_str(), &st) == -1 && mkdir(configDirPath.c_str(), 0755) == -1) {
        Utility::error("Could not create config directory");
        return;
    }

    std::ofstream file(configFilePath);
    if (file.is_open()) {
        file << "device=" << sDevice << "\n";
        file << "button1=" << sButton << "\n";
        file << "button2=" << sButton2 << "\n";
        file << "pttonpath=" << sPttOnPath << "\n";
        file << "pttoffpath=" << sPttOffPath << "\n";
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
        Utility::print(line);
        if (line.find("device=") == 0) {
            sDevice = line.substr(7);
        } else if (line.find("button1=") == 0) {
            ConversionResult result = safeAtoi(line.substr(8));
            if (result.success) {
                sButton = result.value;
            } else {
                Utility::error("Failed to parse button1 value");
            }
        } else if (line.find("button2=") == 0) {
            ConversionResult result = safeAtoi(line.substr(8));
            if (result.success) {
                sButton2 = result.value;
            } else {
                Utility::error("Failed to parse button2 value");
            }
        } else if (line.find("pttonpath=") == 0) {
            sPttOnPath = line.substr(10);
        } else if (line.find("pttoffpath=") == 0) {
            sPttOffPath = line.substr(11);
        }
    }
}
