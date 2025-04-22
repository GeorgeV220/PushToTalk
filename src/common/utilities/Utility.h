#ifndef UTILITY_H
#define UTILITY_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <AL/al.h>
#include <AL/alc.h>

struct UserInfo {
    std::string name;
    std::string path;
    uid_t uid;
    gid_t gid;
};

class Utility {
public:
    // Static members
    static bool debug;
    static ALCdevice* alDevice;
    static ALCcontext* alContext;
    static std::vector<ALuint> sourcePool;
    static std::map<std::string, ALuint> bufferCache;
    static std::mutex audioMutex;

    // Function declarations
    static std::string trim(const std::string &str);
    static std::pair<std::string, std::string> splitKeyValue(const std::string &line);
    static std::vector<std::string> split(const std::string &s, char delimiter);
    static void print(const std::string& message);
    static void error(const std::string& message);
    static void debugPrint(const std::string& message);
    static void pError(const std::string& message);
    static std::string get_active_user();
    static UserInfo get_active_user_info();
};

#endif // UTILITY_H