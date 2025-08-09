#include "PushToTalkApp.h"
#include "common/utilities/Utility.h"
#include "gui/SettingsGUI.h"
#include "utilities/Settings.h"
#include "utilities/AudioUtilities.h"

#include <libappindicator/app-indicator.h>
#include <gtk/gtk.h>
#include <iostream>

PushToTalkApp::PushToTalkApp(const int argc, char *argv[]) {
    Settings::settings.loadSettings();
    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "--debug") {
            Utility::set_debug(true);
        } else if (arg == "--gui") {
            showGui = true;
        }
    }
    Utility::print("Initializing audio system...");
    AudioUtilities::initAudioSystem();
    initializeGtk(argc, argv);
}

PushToTalkApp::~PushToTalkApp() {
    Utility::print("Cleaning up audio system...");
    AudioUtilities::cleanupAudioSystem();
    Utility::print("Cleaning up client...");
    client_.stop();
    Utility::print("Cleaning up virtual microphone...");
    virtualMicrophone_.stop();
}

PushToTalkApp &PushToTalkApp::getInstance(const int argc, char *argv[]) {
    static PushToTalkApp instance(argc, argv);
    return instance;
}

void PushToTalkApp::initializeGtk(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
}

void PushToTalkApp::createTrayIcon() {
    AppIndicator *indicator = app_indicator_new(
        "push-to-talk-indicator",
        "system-run",
        APP_INDICATOR_CATEGORY_APPLICATION_STATUS
    );
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *showGuiItem = gtk_menu_item_new_with_label("Show Settings");
    g_signal_connect(showGuiItem, "activate", G_CALLBACK(onShowSettings), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), showGuiItem);

    GtkWidget *reload = gtk_menu_item_new_with_label("Reload Client");
    g_signal_connect(reload, "activate", G_CALLBACK([]() {
                         auto &ptt_app = PushToTalkApp::getInstance();
                         ptt_app.reload();
                         }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), reload);

    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);

    GtkWidget *exitItem = gtk_menu_item_new_with_label("Exit");
    g_signal_connect(exitItem, "activate", G_CALLBACK(onExit), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), exitItem);

    gtk_widget_show_all(menu);
    app_indicator_set_menu(indicator, GTK_MENU(menu));
}

void PushToTalkApp::onExit(GtkMenuItem *, const gpointer data) {
    if (auto *self = static_cast<PushToTalkApp *>(data)) {
        self->stop();
    }
    gtk_main_quit();
}

void PushToTalkApp::onShowSettings(GtkMenuItem *, gpointer) {
    SettingsGUI::showSettingsGui();
}

void PushToTalkApp::run() {
    createTrayIcon();

    for (const DeviceSettings &device_settings: Settings::settings.devices) {
        client_.add_device(device_settings.getVendorID(), device_settings.getProductID(),
                           device_settings.getDeviceUID(), device_settings.button, device_settings.exclusive);
    }
    try {
        client_.set_callback([](const bool pressed) {
            Utility::debugPrint(
                "Button " + std::string(
                    pressed ? "pressed" : "released"));
            AudioUtilities::playSound(
                (!pressed ? Settings::settings.sPttOffPath : Settings::settings.sPttOnPath).c_str());
            AudioUtilities::setMicMute(!pressed);
        });

        client_.start();
    } catch (const std::exception &e) {
        Utility::error("Error: " + std::string(e.what()));
        return;
    }

    try {
        virtualMicrophone_.set_audio_config(Settings::settings.rate, Settings::settings.channels,
                                            Settings::settings.buffer_frames);
        virtualMicrophone_.set_capture_buffer_size(Settings::settings.capture_buffer_size);
        virtualMicrophone_.set_playback_buffer_size(Settings::settings.playback_buffer_size);
        virtualMicrophone_.set_capture_target("");
        virtualMicrophone_.set_playback_name("ptt_virtual_mic");
        virtualMicrophone_.set_microphone_name("PTT Virtual Microphone");
        Utility::print("Starting virtual microphone...");
        virtualMicrophone_.start();
    } catch (const std::exception &e) {
        Utility::error("Virtual Microphone Error: " + std::string(e.what()));
        return;
    }

    if (showGui) {
        SettingsGUI::showSettingsGui();
    }

    gtk_main();
}

void PushToTalkApp::stop() {
    client_.stop();
    virtualMicrophone_.stop();
}

void PushToTalkApp::reload() {
    Utility::print("Reloading client...");
    client_.clear_devices();
    for (const auto &dev: Settings::settings.devices)
        client_.add_device(dev.getVendorID(), dev.getProductID(), dev.getDeviceUID(), dev.button, dev.exclusive);
    client_.restart();

    virtualMicrophone_.set_audio_config(Settings::settings.rate, Settings::settings.channels,
                                        Settings::settings.buffer_frames);
    virtualMicrophone_.set_capture_buffer_size(Settings::settings.capture_buffer_size);
    virtualMicrophone_.set_playback_buffer_size(Settings::settings.playback_buffer_size);
    virtualMicrophone_.restart();
}
