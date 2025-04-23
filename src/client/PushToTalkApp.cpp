#include "PushToTalkApp.h"
#include "../common/utilities/Utility.h"
#include "gui/SettingsGUI.h"
#include "utilities/Settings.h"
#include <gtk/gtk.h>
#include <libappindicator/app-indicator.h>

#include "../common/utilities/numbers/Conversion.h"
#include "utilities/AudioUtilities.h"

PushToTalkApp::PushToTalkApp(const int argc, char *argv[]) {
    Settings::settings.loadSettings();
    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "--debug") {
            Utility::debug = true;
        } else if (arg == "--gui") {
            showGui = true;
        }
    }
    Utility::print("Initializing audio system...");
    AudioUtilities::initAudioSystem();
    initializeGtk(argc, argv);
}

PushToTalkApp::~PushToTalkApp() {
    if (pushToTalkThread.joinable()) {
        pushToTalkThread.join();
    }
    AudioUtilities::cleanupAudioSystem();
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

    pushToTalkThread = std::thread([this]() {
        client_.set_vendor_id(Settings::settings.getVendorID());
        client_.set_product_id(Settings::settings.getProductID());
        client_.set_device_uid(Settings::settings.getDeviceUID());
        client_.set_target_key(Settings::settings.sButton);
        try {
            client_.set_callback([](const bool pressed) {
                Utility::debugPrint(
                    "Button " + safeIntToStr(Settings::settings.sButton).str + " " + std::string(
                        pressed ? "pressed" : "released"));
                AudioUtilities::playSound(
                    (!pressed ? Settings::settings.sPttOffPath : Settings::settings.sPttOnPath).c_str());
                AudioUtilities::setMicMute(!pressed);
            });

            client_.start();
            while (running) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } catch (const std::exception &e) {
            Utility::error("Error: " + std::string(e.what()));
            return 1;
        }
        return 0;
    });

    if (showGui) {
        SettingsGUI::showSettingsGui();
    }

    gtk_main();
}

void PushToTalkApp::stop() {
    running = false;
    client_.stop();
}
