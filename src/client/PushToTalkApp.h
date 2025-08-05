#pragma once

#include <thread>
#include <gtk/gtk.h>
#include "InputClient.h"
#include "client/utilities/VirtualMicrophone.h"

class PushToTalkApp {
public:
    PushToTalkApp(int argc, char *argv[]);

    ~PushToTalkApp();

    void run();

    void stop();

    InputClient &getClient() { return client_; }

    VirtualMicrophone &getVirtualMicrophone() { return virtualMicrophone_; }

    static PushToTalkApp &getInstance(int argc = 0, char *argv[] = nullptr);

private:
    InputClient client_;

    VirtualMicrophone virtualMicrophone_;

    static void initializeGtk(int argc, char *argv[]);

    void createTrayIcon();

    static void onExit(GtkMenuItem *, gpointer);

    static void onShowSettings(GtkMenuItem *, gpointer);

    bool showGui = false;
};
