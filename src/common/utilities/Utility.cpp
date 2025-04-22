#include "Utility.h"

#include <iostream>
#include <unistd.h>
#include <string>
#include <pwd.h>

#include <mpg123.h>
#include <sstream>

#include <vector>

bool Utility::debug = false;


std::string Utility::trim(const std::string &str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) start++;
    auto end = str.end();
    while (end != start && std::isspace(*(end - 1))) end--;
    return std::string(start, end);
}

std::pair<std::string, std::string> Utility::splitKeyValue(const std::string &line) {
    const size_t eqPos = line.find('=');
    if (eqPos == std::string::npos) return {"", ""};

    std::string key = trim(line.substr(0, eqPos));
    std::string value = trim(line.substr(eqPos + 1));
    return {key, value};
}

std::vector<std::string> Utility::split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void Utility::print(const std::string &message) {
    std::cout << message << std::endl;
}

void Utility::error(const std::string &message) {
    std::cerr << "Error: " << message << std::endl;
}

void Utility::debugPrint(const std::string &message) {
    if (debug) {
        std::cout << "Debug: " << message << std::endl;
    }
}

void Utility::pError(const std::string &message) {
    if (debug) {
        ::perror(message.c_str());
    }
}

std::string Utility::get_active_user() {
    if (const char *sudoUser = getenv("SUDO_USER")) {
        return std::string(sudoUser);
    }

    const uid_t uid = getuid();
    if (const passwd *pw = getpwuid(uid)) {
        return std::string(pw->pw_name);
    }

    return "";
}

UserInfo Utility::get_active_user_info() {
    const char *sudoUser = getenv("SUDO_USER");
    const uid_t uid = getuid();

    if (sudoUser) {
        if (const passwd *pw = getpwnam(sudoUser)) {
            return {sudoUser, pw->pw_dir, pw->pw_uid, pw->pw_gid};
        }
    }

    if (const passwd *pw = getpwuid(uid)) {
        return {pw->pw_name, pw->pw_dir, uid, pw->pw_gid};
    }

    throw std::runtime_error("Failed to determine user info");
}
