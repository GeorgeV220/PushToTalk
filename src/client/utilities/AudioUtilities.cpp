#include "AudioUtilities.h"

#include <unistd.h>
#include <map>
#include <string>
#include <pwd.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <mpg123.h>
#include <mutex>
#include <sstream>
#include <pulse/pulseaudio.h>

#include <thread>
#include <vector>
#include <atomic>

#include "Settings.h"
#include "common/utilities/Utility.h"


ALCdevice *AudioUtilities::alDevice = nullptr;
ALCcontext *AudioUtilities::alContext = nullptr;
std::vector<ALuint> AudioUtilities::sourcePool;
std::map<std::string, ALuint> AudioUtilities::bufferCache;
std::mutex AudioUtilities::audioMutex;

struct CallbackData {
    std::atomic<bool> micMute;
    pa_mainloop *ml{};
};

static void context_state_cb(pa_context *c, void *userdata) {
    const auto *data = static_cast<CallbackData *>(userdata);
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY: {
            pa_operation *op = pa_context_get_server_info(
                c, [](pa_context *c, const pa_server_info *i, void *userdata) {
                    const auto *data = static_cast<CallbackData *>(userdata);
                    if (!i) {
                        pa_mainloop_quit(data->ml, 1);
                        return;
                    }
                    pa_operation *op = pa_context_get_source_info_by_name(
                        c, i->default_source_name, [](pa_context *c, const pa_source_info *i, int eol, void *userdata) {
                            auto *data = static_cast<CallbackData *>(userdata);
                            if (eol < 0) {
                                Utility::error("Failed to get source info");
                                pa_mainloop_quit(data->ml, 1);
                                return;
                            }
                            if (eol) return;
                            pa_operation *set_op = pa_context_set_source_mute_by_index(
                                c, i->index, data->micMute, [](pa_context *c, int success, void *userdata) {
                                    const auto *data = static_cast<CallbackData *>(userdata);
                                    if (!success) {
                                        Utility::error("Failed to set mute state");
                                    }
                                    pa_mainloop_quit(data->ml, 0);
                                }, userdata);
                            pa_operation_unref(set_op);
                        }, userdata);
                    pa_operation_unref(op);
                }, userdata);
            pa_operation_unref(op);
            break;
        }
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_mainloop_quit(data->ml, 1);
            break;
        default:
            break;
    }
}

void AudioUtilities::setMicMute(const bool micMute) {
    const UserInfo activeUser = Utility::get_active_user_info();

    if (setuid(activeUser.uid) != 0) {
        Utility::error("Failed to switch to user " + activeUser.name);
        return;
    }

    setenv("HOME", activeUser.path.c_str(), 1);

    const std::string runtimeDir = "/run/user/" + std::to_string(activeUser.uid);
    setenv("XDG_RUNTIME_DIR", runtimeDir.c_str(), 1);

    CallbackData data;
    data.micMute = micMute;
    data.ml = pa_mainloop_new();
    pa_mainloop_api *api = pa_mainloop_get_api(data.ml);
    pa_context *ctx = pa_context_new(api, "MicMuteUtility");

    pa_context_set_state_callback(ctx, context_state_cb, &data);
    pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    int ret = 0;
    do {
        if (pa_mainloop_iterate(data.ml, 1, &ret) < 0) {
            break;
        }
    } while (ret == 0);

    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(data.ml);
}

void AudioUtilities::initAudioSystem() {
    if (mpg123_init() != MPG123_OK) {
        Utility::error("Failed to initiate mpg123");
        return;
    }

    alDevice = alcOpenDevice(nullptr);
    if (!alDevice) {
        Utility::error("Failed to open audio device");
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

void AudioUtilities::cleanupAudioSystem() {
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

void AudioUtilities::playSound(const char *fileName) {
    std::lock_guard lock(audioMutex);

    ALuint bufferId;
    if (const auto bufferIt = bufferCache.find(fileName); bufferIt != bufferCache.end()) {
        bufferId = bufferIt->second;
    } else {
        mpg123_handle *mh = mpg123_new(nullptr, nullptr);
        if (!mh || mpg123_open(mh, fileName) != MPG123_OK) {
            Utility::error("Failed to open sound file");
            if (mh) mpg123_delete(mh);
            return;
        }

        long rate;
        int channels, encoding;
        if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
            Utility::error("Failed to get audio format");
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
        Utility::debugPrint("No available audio sources - skipping playback");
        return;
    }

    ALuint source = sourcePool.back();
    sourcePool.pop_back();

    alSourcei(source, AL_BUFFER, bufferId);
    alSourcef(source, AL_GAIN, Settings::settings.sVolume);
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
