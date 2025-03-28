#ifndef UTILITY_H
#define UTILITY_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <AL/al.h>
#include <AL/alc.h>

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
    static void print(const std::string& message);
    static void error(const std::string& message);
    static void debugPrint(const std::string& message);
    static void pError(const std::string& message);
    static std::string get_active_user();
    static void setMicMute(bool micMute);
    static void initAudioSystem();
    static void cleanupAudioSystem();
    static void playSound(const char* fileName);
};

#endif // UTILITY_H