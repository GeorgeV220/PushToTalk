#ifndef UTILITY_H
#define UTILITY_H

#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <pwd.h>

class Utility {
public:
    static bool debug;

    static void print(const std::string &message) {
        std::cout << message << std::endl;
    }

    static void error(const std::string &message) {
        std::cerr << "Error: " << message << std::endl;
    }

    static void debugPrint(const std::string &message) {
        if (debug) {
            std::cout << "Debug: " << message << std::endl;
        }
    }

    static void pError(const std::string &message) {
        if (debug) {
            ::perror(message.c_str());
        }
    }

    static std::string get_active_user() {
        if (const char *sudoUser = getenv("SUDO_USER")) {
            return std::string(sudoUser);
        }

        // Fallback: Get the current real user ID
        const uid_t uid = getuid();
        if (const passwd *pw = getpwuid(uid)) {
            return std::string(pw->pw_name);
        }

        return "";
    }

    static void setMicMute(const bool micMute) {
        const std::string activeUser = get_active_user();
        if (activeUser.empty()) {
            error("Unable to determine the active user");
            return;
        }

        const passwd *pw = getpwnam(activeUser.c_str());
        if (!pw) {
            error("Failed to retrieve user details for " + activeUser);
            return;
        }

        const uid_t userUID = pw->pw_uid;
        const char *userHome = pw->pw_dir;

        if (setuid(userUID) != 0) {
            error("Failed to switch to user " + activeUser);
            return;
        }

        setenv("HOME", userHome, 1);

        const std::string runtimeDir = "/run/user/" + std::to_string(userUID);
        setenv("XDG_RUNTIME_DIR", runtimeDir.c_str(), 1);

        const std::string command = micMute
                                        ? "pactl set-source-mute @DEFAULT_SOURCE@ 1"
                                        : "pactl set-source-mute @DEFAULT_SOURCE@ 0";

        if (system(command.c_str()) == -1) {
            error("Failed to execute pactl command");
        }
    }
};

#endif // UTILITY_H
