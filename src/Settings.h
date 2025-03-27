#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

struct ConversionResult {
    int value;
    bool success;

    ConversionResult() : value(-1), success(false) {
    }
};

class Settings {
private:
    std::string configFilePath;
    std::string configDirPath;

public:
    std::string sDevice;
    int sButton;
    int sButton2;
    std::string sPttOnPath;
    std::string sPttOffPath;
    float sVolume;

    Settings();

    static ConversionResult safeAtoi(const std::string &str);

    void loadSettings();

    void saveSettings(const std::string &device, int button1, int button2, std::string pttOnPath, std::string pttOffPath, float volume);

    static void setPath(const std::string &path);
};

#endif // SETTINGS_H
