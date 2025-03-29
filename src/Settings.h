#ifndef SETTINGS_H
#define SETTINGS_H

#include <cstdint>
#include <string>

class Settings {
private:
    std::string configFilePath;
    std::string configDirPath;

public:
    static Settings settings;
    std::string sDevice;
    int sButton;
    std::string sPttOnPath;
    std::string sPttOffPath;
    float sVolume;

    Settings();

    void loadSettings();

    void saveSettings(const std::string &device, int button, std::string pttOnPath,
                      std::string pttOffPath, float volume);

    [[nodiscard]] uint16_t getVendorID() const;
    [[nodiscard]] uint16_t getProductID() const;
    [[nodiscard]] uint32_t getDeviceUID() const;

    static void setPath(const std::string &path);
};

#endif // SETTINGS_H
