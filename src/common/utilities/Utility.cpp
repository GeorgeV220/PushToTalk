#include "Utility.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <string>
#include <pwd.h>

#include <mpg123.h>
#include <sstream>

#include <vector>


void Utility::set_debug(const bool enabled) {
    debug_enabled_ = enabled;
}

bool Utility::is_debug_enabled() {
    return debug_enabled_;
}

std::string Utility::trim(const std::string &str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) ++start;
    auto end = str.end();
    while (end != start && std::isspace(*(end - 1))) --end;
    return {start, end};
}

std::pair<std::string, std::string> Utility::splitKeyValue(const std::string &line) {
    const size_t eqPos = line.find('=');
    if (eqPos == std::string::npos) return {"", ""};

    std::string key = trim(line.substr(0, eqPos));
    std::string value = trim(line.substr(eqPos + 1));
    return {key, value};
}

std::vector<std::string> Utility::split(const std::string &s, const char delimiter) {
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
    if (is_debug_enabled()) {
        std::cout << "Debug: " << message << std::endl;
    }
}

void Utility::pError(const std::string &message) {
    if (is_debug_enabled()) {
        ::perror(message.c_str());
    }
}

// TODO switch to nss
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

/**
 *  Safely reads from fd and stores it in buffer.
 *  Returns size on success.
 *  Returns -1 on error and sets errno.
 *  Returns -2 on mid-read EOF.
 */
ssize_t Utility::safe_read(const int fd, void *buffer, const size_t size) {
    const auto ptr = static_cast<char *>(buffer);
    ssize_t been_read = 0;
    while (size > been_read) {
        const ssize_t curr = read(fd, ptr + been_read, size - been_read);
        if (curr == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (curr == 0) break;
        been_read += curr;
    }
    return been_read;
}

/**
 *  Safely writes to fd from buffer.
 *  Returns size on success.
 *  Returns -1 on error and sets errno.
 */
ssize_t Utility::safe_write(const int fd, const void *buffer, const size_t size) {
    const auto ptr = static_cast<const char *>(buffer);
    ssize_t been_send = 0;
    while (size > been_send) {
        /* While not at buffer end */
        const ssize_t curr = write(fd, ptr + been_send, size - been_send);
        if (curr == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        been_send += curr;
    }
    return been_send;
}
