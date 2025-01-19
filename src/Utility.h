#ifndef UTILITY_H
#define UTILITY_H

#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <pwd.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <mpg123.h>
#include <vector>

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

    static void playSound(const char *fileName) {

        if (mpg123_init() != MPG123_OK) {
            error("Failed to initiate mpg123");
            return;
        }

        mpg123_handle *mh = mpg123_new(nullptr, nullptr);
        if (!mh) {
            error("Failed to create mpg123 handle");
            return;
        }

        if (mpg123_open(mh, fileName) != MPG123_OK) {
            error("Failed to open sound file");
            mpg123_delete(mh);
            return;
        }

        long rate;
        int channels, encoding;
        if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
            error("Failed to retrieve sound file format");
            mpg123_close(mh);
            mpg123_delete(mh);
            return;
        }

        std::vector<unsigned char> buffer(4096);
        std::vector<unsigned char> pcm_data;
        size_t done;
        int err;

        while ((err = mpg123_read(mh, buffer.data(), buffer.size(), &done)) == MPG123_OK) {
            pcm_data.insert(pcm_data.end(), buffer.begin(), buffer.begin() + done);
        }

        if (err != MPG123_DONE && err != MPG123_OK) {
            error("Failed to read sound file");
            mpg123_close(mh);
            mpg123_delete(mh);
            mpg123_exit();
            return;
        }

        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();

        ALCdevice *device = alcOpenDevice(nullptr);
        if (!device) {
            error("Failed to open audio device");
            return;
        }

        ALCcontext *context = alcCreateContext(device, nullptr);
        alcMakeContextCurrent(context);

        ALuint buffer_id;
        ALuint source;

        alGenBuffers(1, &buffer_id);
        alGenSources(1, &source);

        const ALenum format = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        alBufferData(buffer_id, format, pcm_data.data(), pcm_data.size(), rate);

        alSourcei(source, AL_BUFFER, buffer_id);

        alSourcef(source, AL_GAIN, 0.1f);

        alSourcePlay(source);

        ALint state;
        do {
            alGetSourcei(source, AL_SOURCE_STATE, &state);
        } while (state == AL_PLAYING);

        alDeleteSources(1, &source);
        alDeleteBuffers(1, &buffer_id);
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        alcCloseDevice(device);
    }
};

#endif // UTILITY_H
