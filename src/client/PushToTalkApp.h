#pragma once
#include <thread>
#include <gtk/gtk.h>
#include "InputClient.h"

class PushToTalkApp {
public:
    PushToTalkApp(int argc, char *argv[]);

    ~PushToTalkApp();

    void run();

    void stop();

    InputClient &getClient() { return client_; }

    static PushToTalkApp &getInstance(int argc = 0, char *argv[] = nullptr);

private:
    InputClient client_;

    static void initializeGtk(int argc, char *argv[]);

    void createTrayIcon();

    static void onExit(GtkMenuItem *, gpointer);

    static void onShowSettings(GtkMenuItem *, gpointer);

    std::thread pushToTalkThread;
    bool showGui = false;
    std::atomic<bool> running{false};
};
