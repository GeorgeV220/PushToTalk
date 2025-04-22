#ifndef AUDIOUTILITIES_H
#define AUDIOUTILITIES_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <AL/al.h>
#include <AL/alc.h>

class AudioUtilities {
public:
    static ALCdevice *alDevice;
    static ALCcontext *alContext;
    static std::vector<ALuint> sourcePool;
    static std::map<std::string, ALuint> bufferCache;
    static std::mutex audioMutex;

    static void setMicMute(bool micMute);
    static void initAudioSystem();
    static void cleanupAudioSystem();
    static void playSound(const char* fileName);
};


#endif //AUDIOUTILITIES_H
