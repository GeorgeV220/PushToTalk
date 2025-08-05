#include "VirtualMicrophone.h"
#include "common/utilities/Utility.h"
#include <cstring>
#include <iostream>
#include <utility>

const pw_stream_events VirtualMicrophone::capture_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .state_changed = VirtualMicrophone::capture_state_changed,
        .param_changed = VirtualMicrophone::capture_param_changed,
        .process = VirtualMicrophone::capture_process,
};

const pw_stream_events VirtualMicrophone::playback_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .param_changed = VirtualMicrophone::playback_param_changed,
        .process = VirtualMicrophone::playback_process
};

VirtualMicrophone::VirtualMicrophone(
        std::string capture_target,
        std::string playback_name,
        std::string microphone_name,
        uint32_t rate,
        uint32_t channels
) : loop_(nullptr),
    capture_stream_(nullptr),
    playback_stream_(nullptr),
    buffer_(nullptr),
    buffer_frames_(16384),
    write_pos_(0),
    read_pos_(0),
    frames_available_(0),
    capture_target_(std::move(capture_target)),
    playback_name_(std::move(playback_name)),
    microphone_name_(std::move(microphone_name)),
    rate_(rate),
    channels_(channels),
    running_(false) {
    initialize_pipewire();
    create_streams();
}

VirtualMicrophone::~VirtualMicrophone() {
    cleanup();
}

void VirtualMicrophone::initialize_pipewire() {
    pw_init(nullptr, nullptr);
    loop_ = pw_main_loop_new(nullptr);
    if (!loop_) {
        throw std::runtime_error("Failed to create PipeWire main loop");
    }

    buffer_ = new float[buffer_frames_ * channels_]();
}

void VirtualMicrophone::create_streams() {
    pw_properties *capture_props = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Music",
            nullptr
    );
    if (!capture_target_.empty()) {
        pw_properties_set(capture_props, PW_KEY_TARGET_OBJECT, capture_target_.c_str());
    }

    capture_stream_ = pw_stream_new_simple(
            pw_main_loop_get_loop(loop_),
            "audio-capture",
            capture_props,
            &capture_events,
            this
    );

    pw_properties *playback_props = pw_properties_new(
            PW_KEY_MEDIA_CLASS, "Audio/Source",
            PW_KEY_NODE_NAME, playback_name_.c_str(),
            PW_KEY_NODE_DESCRIPTION, microphone_name_.c_str(),
            nullptr
    );

    playback_stream_ = pw_stream_new_simple(
            pw_main_loop_get_loop(loop_),
            "audio-playback",
            playback_props,
            &playback_events,
            this
    );

    uint8_t capture_buffer[1024];
    spa_pod_builder capture_builder = SPA_POD_BUILDER_INIT(capture_buffer, sizeof(capture_buffer));

    spa_audio_info_raw capture_info = SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F32,
            .rate = rate_,
            .channels = channels_
    );

    const spa_pod *capture_params[1] = {
            spa_format_audio_raw_build(&capture_builder, SPA_PARAM_EnumFormat, &capture_info)
    };

    pw_stream_connect(capture_stream_,
                      PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                   PW_STREAM_FLAG_MAP_BUFFERS |
                                                   PW_STREAM_FLAG_RT_PROCESS |
                                                   PW_STREAM_FLAG_INACTIVE),
                      capture_params, 1);
    pw_stream_set_active(capture_stream_, true);

    uint8_t playback_buffer[1024];
    spa_pod_builder playback_builder = SPA_POD_BUILDER_INIT(playback_buffer, sizeof(playback_buffer));

    spa_audio_info_raw playback_info = SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F32,
            .rate = rate_,
            .channels = channels_
    );

    const spa_pod *playback_params[1] = {
            spa_format_audio_raw_build(&playback_builder, SPA_PARAM_EnumFormat, &playback_info)
    };

    pw_stream_connect(playback_stream_,
                      PW_DIRECTION_OUTPUT,
                      PW_ID_ANY,
                      static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                   PW_STREAM_FLAG_MAP_BUFFERS |
                                                   PW_STREAM_FLAG_RT_PROCESS |
                                                   PW_STREAM_FLAG_INACTIVE),
                      playback_params, 1);
    pw_stream_set_active(playback_stream_, true);

    pw_loop_add_signal(pw_main_loop_get_loop(loop_), SIGINT, do_quit, this);
    pw_loop_add_signal(pw_main_loop_get_loop(loop_), SIGTERM, do_quit, this);
}

void VirtualMicrophone::start() {
    if (!running_) {
        running_ = true;
        pw_main_loop_run(loop_);
    }
}

void VirtualMicrophone::stop() {
    if (running_) {
        running_ = false;
        pw_main_loop_quit(loop_);
    }
}

void VirtualMicrophone::cleanup() {
    if (capture_stream_) pw_stream_destroy(capture_stream_);
    if (playback_stream_) pw_stream_destroy(playback_stream_);
    if (loop_) pw_main_loop_destroy(loop_);
    delete[] buffer_;
    pw_deinit();
}

void VirtualMicrophone::buffer_write(const float *src, uint32_t n_frames) {
    uint32_t free_space = buffer_frames_ - frames_available_;
    if (n_frames > free_space) {
        Utility::error(
                "Buffer overrun: Requested " + std::to_string(n_frames) + ", only got " + std::to_string(free_space));
        Utility::error("Dropping " + std::to_string(n_frames - free_space) + " frames");
        read_pos_ = (read_pos_ + (n_frames - free_space)) % buffer_frames_;
        frames_available_ -= (n_frames - free_space);
    }

    for (uint32_t i = 0; i < n_frames; ++i) {
        uint32_t pos = (write_pos_ + i) % buffer_frames_;
        for (uint32_t c = 0; c < channels_; ++c) {
            buffer_[pos * channels_ + c] = src[i * channels_ + c];
        }
    }
    write_pos_ = (write_pos_ + n_frames) % buffer_frames_;
    frames_available_ += n_frames;
}

void VirtualMicrophone::buffer_read(float *dst, uint32_t n_frames) {
    uint32_t frames_to_read = std::min(n_frames, frames_available_);

    if (frames_to_read < n_frames) {
        Utility::error(
                "Buffer underrun: Requested " + std::to_string(n_frames) + ", only got " +
                std::to_string(frames_to_read));
    }

    for (uint32_t i = 0; i < frames_to_read; ++i) {
        uint32_t pos = (read_pos_ + i) % buffer_frames_;
        for (uint32_t c = 0; c < channels_; ++c) {
            dst[i * channels_ + c] = buffer_[pos * channels_ + c];
        }
    }

    if (frames_to_read < n_frames) {
        std::memset(&dst[frames_to_read * channels_], 0, (n_frames - frames_to_read) * channels_ * sizeof(float));
    }

    read_pos_ = (read_pos_ + frames_to_read) % buffer_frames_;
    frames_available_ -= frames_to_read;
}

void VirtualMicrophone::on_capture_process() {
    pw_buffer *buf = pw_stream_dequeue_buffer(capture_stream_);
    if (!buf) return;

    spa_buffer *spa_buf = buf->buffer;
    auto *data = static_cast<float *>(spa_buf->datas[0].data);
    if (!data) {
        pw_stream_queue_buffer(capture_stream_, buf);
        return;
    }

    uint32_t n_samples = spa_buf->datas[0].chunk->size / sizeof(float);
    buffer_write(data, n_samples / channels_);
    pw_stream_queue_buffer(capture_stream_, buf);
}

void VirtualMicrophone::on_playback_process() {
    pw_buffer *buf = pw_stream_dequeue_buffer(playback_stream_);
    if (!buf) return;

    spa_buffer *spa_buf = buf->buffer;
    auto *data = static_cast<float *>(spa_buf->datas[0].data);
    if (!data) {
        pw_stream_queue_buffer(playback_stream_, buf);
        return;
    }

    uint32_t stride = sizeof(float) * channels_;
    uint32_t max_frames = spa_buf->datas[0].maxsize / stride;
    uint32_t req_frames = buf->requested ? std::min(static_cast<uint32_t>(buf->requested), max_frames) : max_frames;

    buffer_read(data, req_frames);

    spa_buf->datas[0].chunk->offset = 0;
    spa_buf->datas[0].chunk->stride = stride;
    spa_buf->datas[0].chunk->size = req_frames * stride;

    pw_stream_queue_buffer(playback_stream_, buf);
}

void VirtualMicrophone::on_capture_param_changed(uint32_t id, const spa_pod *param) {
    if (!param || id != SPA_PARAM_Format) return;

    if (spa_format_parse(param, &format_.media_type, &format_.media_subtype) < 0) return;
    if (format_.media_type != SPA_MEDIA_TYPE_audio || format_.media_subtype != SPA_MEDIA_SUBTYPE_raw) return;

    spa_format_audio_raw_parse(param, &format_.info.raw);
    Utility::print("Capture format: rate=" + std::to_string(format_.info.raw.rate)
                   + " channels=" + std::to_string(format_.info.raw.channels));
}

void VirtualMicrophone::on_capture_state_changed(pw_stream_state old, pw_stream_state state, const char *error) {
    Utility::print("Capture stream state changed: " + std::string(pw_stream_state_as_string(state)));
    if (error) {
        Utility::error("Error: " + std::string(error));
    }
    if (state == PW_STREAM_STATE_PAUSED || state == PW_STREAM_STATE_UNCONNECTED) {
        write_pos_ = 0;
        read_pos_ = 0;
        frames_available_ = 0;
    }
}

void VirtualMicrophone::capture_process(void *userdata) {
    static_cast<VirtualMicrophone *>(userdata)->on_capture_process();
}

void VirtualMicrophone::playback_process(void *userdata) {
    static_cast<VirtualMicrophone *>(userdata)->on_playback_process();
}

void VirtualMicrophone::playback_param_changed(void *userdata, uint32_t id, const spa_pod *param) {
    if (!param || id != SPA_PARAM_Format) return;
    spa_audio_info_raw info;
    spa_format_audio_raw_parse(param, &info);
    Utility::print("Playback format: rate=" + std::to_string(info.rate)
                   + " channels=" + std::to_string(info.channels));
}

void VirtualMicrophone::capture_param_changed(void *userdata, uint32_t id, const spa_pod *param) {
    static_cast<VirtualMicrophone *>(userdata)->on_capture_param_changed(id, param);
}

void VirtualMicrophone::capture_state_changed(void *userdata, pw_stream_state old, pw_stream_state state,
                                              const char *error) {
    static_cast<VirtualMicrophone *>(userdata)->on_capture_state_changed(old, state, error);
}

void VirtualMicrophone::do_quit(void *userdata, int signal_number) {
    static_cast<VirtualMicrophone *>(userdata)->stop();
}