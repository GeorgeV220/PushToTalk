#include "Utility.h"

#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <map>
#include <string>
#include <pwd.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <mpg123.h>
#include <mutex>
#include <thread>
#include <vector>

#include "Settings.h"

bool Utility::debug = false;
Settings Utility::settings;

ALCdevice *Utility::alDevice = nullptr;
ALCcontext *Utility::alContext = nullptr;
std::vector<ALuint> Utility::sourcePool;
std::map<std::string, ALuint> Utility::bufferCache;
std::mutex Utility::audioMutex;

static Settings settings;

static ALCdevice *alDevice;
static ALCcontext *alContext;
static std::vector<ALuint> sourcePool;
static std::map<std::string, ALuint> bufferCache;
static std::mutex audioMutex;

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

    // Fallback: Get the current real user ID
    const uid_t uid = getuid();
    if (const passwd *pw = getpwuid(uid)) {
        return std::string(pw->pw_name);
    }

    return "";
}

void Utility::setMicMute(const bool micMute) {
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

void Utility::initAudioSystem() {
    if (mpg123_init() != MPG123_OK) {
        error("Failed to initiate mpg123");
        return;
    }

    alDevice = alcOpenDevice(nullptr);
    if (!alDevice) {
        error("Failed to open audio device");
        return;
    }

    alContext = alcCreateContext(alDevice, nullptr);
    alcMakeContextCurrent(alContext);

    ALuint sources[5];
    alGenSources(5, sources);
    for (int i = 0; i < 5; ++i) {
        sourcePool.push_back(sources[i]);
    }
}

void Utility::cleanupAudioSystem() {
    std::lock_guard lock(audioMutex);

    for (ALuint source: sourcePool) {
        alSourceStop(source);
        alDeleteSources(1, &source);
    }
    sourcePool.clear();

    for (auto &entry: bufferCache) {
        alDeleteBuffers(1, &entry.second);
    }
    bufferCache.clear();

    alcMakeContextCurrent(nullptr);
    if (alContext) alcDestroyContext(alContext);
    if (alDevice) alcCloseDevice(alDevice);
    mpg123_exit();
}

void Utility::playSound(const char *fileName) {
    std::lock_guard lock(audioMutex);

    ALuint bufferId;
    if (const auto bufferIt = bufferCache.find(fileName); bufferIt != bufferCache.end()) {
        bufferId = bufferIt->second;
    } else {
        mpg123_handle *mh = mpg123_new(nullptr, nullptr);
        if (!mh || mpg123_open(mh, fileName) != MPG123_OK) {
            error("Failed to open sound file");
            if (mh) mpg123_delete(mh);
            return;
        }

        long rate;
        int channels, encoding;
        if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
            error("Failed to get audio format");
            mpg123_close(mh);
            mpg123_delete(mh);
            return;
        }

        std::vector<unsigned char> pcm_data;
        size_t done;
        unsigned char buffer[4096];

        while (mpg123_read(mh, buffer, sizeof(buffer), &done) == MPG123_OK) {
            pcm_data.insert(pcm_data.end(), buffer, buffer + done);
        }

        mpg123_close(mh);
        mpg123_delete(mh);

        ALenum format = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        alGenBuffers(1, &bufferId);
        alBufferData(bufferId, format, pcm_data.data(), pcm_data.size(), rate);
        bufferCache[fileName] = bufferId;
    }

    if (sourcePool.empty()) {
        debugPrint("No available audio sources - skipping playback");
        return;
    }

    ALuint source = sourcePool.back();
    sourcePool.pop_back();

    alSourcei(source, AL_BUFFER, bufferId);
    alSourcef(source, AL_GAIN, settings.sVolume);
    alSourcePlay(source);

    std::thread([source]() {
        ALint state;
        do {
            usleep(10000);
            alGetSourcei(source, AL_SOURCE_STATE, &state);
        } while (state == AL_PLAYING);

        std::lock_guard lock(audioMutex);
        alSourceStop(source);
        alSourcei(source, AL_BUFFER, 0);
        sourcePool.push_back(source);
    }).detach();
}
