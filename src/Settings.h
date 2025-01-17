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

    Settings();

    static ConversionResult safeAtoi(const std::string &str);

    void loadSettings();

    void saveSettings(const std::string &device, int button1, int button2);

    static void setPath(const std::string &path);
};

#endif // SETTINGS_H
