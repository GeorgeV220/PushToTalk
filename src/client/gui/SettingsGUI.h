#ifndef SETTINGSGUI_H
#define SETTINGSGUI_H

#include <gtk/gtk.h>

class SettingsGUI {
public:
    static void showSettingsGui();

    static GtkWidget *settingsWindow;
};

#endif // SETTINGSGUI_H
