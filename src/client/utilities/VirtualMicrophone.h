#ifndef PUSHTOTALK_VIRTUALMICROPHONE_H
#define PUSHTOTALK_VIRTUALMICROPHONE_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <string>
#include <stdexcept>
#include <atomic>
#include <thread>

class VirtualMicrophone {
public:
    VirtualMicrophone();

    ~VirtualMicrophone();

    void start();

    void stop();

    void restart();

    void set_capture_target(std::string target);
    void set_playback_name(std::string name);
    void set_microphone_name(std::string name);
    void set_audio_config(uint32_t rate, uint32_t channels, uint32_t buffer_frames);


private:
    void initialize_pipewire();

    void create_streams();

    void cleanup_in_loop();

    void final_cleanup();

    void buffer_write(const float *src, uint32_t n_frames);

    void buffer_read(float *dst, uint32_t n_frames);

    void on_capture_process();

    void on_playback_process();

    void on_capture_param_changed(uint32_t id, const struct spa_pod *param);

    void on_capture_state_changed(pw_stream_state old, pw_stream_state state, const char *error);

    static void capture_process(void *userdata);

    static void playback_process(void *userdata);

    static void playback_param_changed(void *userdata, uint32_t id, const struct spa_pod *param);

    static void capture_param_changed(void *userdata, uint32_t id, const struct spa_pod *param);

    static void capture_state_changed(void *userdata, pw_stream_state old, pw_stream_state state, const char *error);

    static void do_quit(void *userdata, int signal_number);

    pw_main_loop *loop_;
    pw_stream *capture_stream_;
    pw_stream *playback_stream_;

    spa_audio_info format_{};

    float *buffer_;
    uint32_t buffer_frames_;
    uint32_t write_pos_;
    uint32_t read_pos_;
    uint32_t frames_available_;

    std::string capture_target_;
    std::string playback_name_;
    std::string microphone_name_;
    uint32_t rate_;
    uint32_t channels_;
    std::thread listener_thread_;
    std::atomic<bool> running_{false};

    static const pw_stream_events capture_events;
    static const pw_stream_events playback_events;
};

#endif //PUSHTOTALK_VIRTUALMICROPHONE_H
