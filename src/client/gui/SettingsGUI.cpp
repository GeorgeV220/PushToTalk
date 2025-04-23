#include <fcntl.h>
#include <gtk/gtk.h>
#include <iostream>
#include <linux/input.h>
#include <mutex>

#include "SettingsGUI.h"
#include "../PushToTalkApp.h"
#include "../utilities/Settings.h"
#include "../../common/utilities/Utility.h"
#include "../../common/utilities/numbers/Conversion.h"

std::mutex gtk_mutex;
GtkWidget *SettingsGUI::settingsWindow = nullptr;

struct ButtonData {
    GtkEntry **entries;
    bool hideWindow;
    GtkWidget *window;
};

gboolean safe_save_settings(const gpointer data) {
    std::lock_guard lock(gtk_mutex);
    const auto *buttonData = static_cast<ButtonData *>(data);

    const char *device = gtk_entry_get_text(buttonData->entries[0]);
    const IntConversionResult buttonResult = safeStrToInt(gtk_entry_get_text(buttonData->entries[1]));
    const std::string pttOnPath = gtk_entry_get_text(buttonData->entries[2]);
    const std::string pttOffPath = gtk_entry_get_text(buttonData->entries[3]);

    float volume;
    try {
        volume = std::stof(gtk_entry_get_text(buttonData->entries[4]));
    } catch (...) {
        Utility::error("Invalid volume value");
        return G_SOURCE_REMOVE;
    }

    if (buttonResult.success) {
        Settings::settings.saveSettings(
            device,
            buttonResult.value,
            pttOnPath,
            pttOffPath,
            volume
        );

        if (buttonData->hideWindow) {
            gtk_widget_hide(buttonData->window);
        }

        InputClient &client = PushToTalkApp::getInstance().getClient();

        client.set_product_id(Settings::settings.getProductID());
        client.set_vendor_id(Settings::settings.getVendorID());
        client.set_device_uid(Settings::settings.getDeviceUID());
        client.set_target_key(Settings::settings.sButton);
        client.restart();
    } else {
        Utility::error("Invalid button value.");
    }
    return G_SOURCE_REMOVE;
}


void onApplyButtonClicked(const GtkButton *button, const gpointer data) {
    (void) button;
    auto *buttonData = static_cast<ButtonData *>(data);

    gdk_threads_add_idle(safe_save_settings, buttonData);
}

void onWindowDestroy(const GtkWidget *widget, const gpointer data) {
    std::lock_guard lock(gtk_mutex);
    SettingsGUI::settingsWindow = nullptr;

    const auto *buttonData = static_cast<ButtonData *>(data);
    delete[] buttonData->entries;
    delete buttonData;
}

void SettingsGUI::showSettingsGui() {
    std::lock_guard lock(gtk_mutex);

    if (settingsWindow) {
        gtk_window_present(GTK_WINDOW(settingsWindow));
        return;
    }

    settingsWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(settingsWindow), "Settings");
    gtk_window_set_default_size(GTK_WINDOW(settingsWindow), 400, 200);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(settingsWindow), 10);
    gtk_container_add(GTK_CONTAINER(settingsWindow), grid);

    // Device row
    GtkWidget *deviceLabel = gtk_label_new("Device:");
    GtkWidget *deviceEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(deviceEntry), Settings::settings.sDevice.c_str());
    gtk_grid_attach(GTK_GRID(grid), deviceLabel, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), deviceEntry, 1, 0, 1, 1);

    // Button row
    GtkWidget *buttonLabel = gtk_label_new("Button:");
    GtkWidget *buttonEntry = gtk_entry_new();
    const std::string buttonText = std::to_string(Settings::settings.sButton);
    gtk_entry_set_text(GTK_ENTRY(buttonEntry), buttonText.c_str());
    gtk_grid_attach(GTK_GRID(grid), buttonLabel, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), buttonEntry, 1, 1, 1, 1);

    // PTT On row
    GtkWidget *pttOnLabel = gtk_label_new("PTT on:");
    GtkWidget *pttOnEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(pttOnEntry), Settings::settings.sPttOnPath.c_str());
    gtk_grid_attach(GTK_GRID(grid), pttOnLabel, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), pttOnEntry, 1, 2, 1, 1);

    // PTT Off row
    GtkWidget *pttOffLabel = gtk_label_new("PTT off:");
    GtkWidget *pttOffEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(pttOffEntry), Settings::settings.sPttOffPath.c_str());
    gtk_grid_attach(GTK_GRID(grid), pttOffLabel, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), pttOffEntry, 1, 3, 1, 1);

    // Volume row
    GtkWidget *volumeLabel = gtk_label_new("Volume:");
    GtkWidget *volumeEntry = gtk_entry_new();
    const std::string volumeText = std::to_string(Settings::settings.sVolume);
    gtk_entry_set_text(GTK_ENTRY(volumeEntry), volumeText.c_str());
    gtk_grid_attach(GTK_GRID(grid), volumeLabel, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), volumeEntry, 1, 4, 1, 1);

    // Buttons row
    GtkWidget *applyAndHideButton = gtk_button_new_with_label("Apply and Hide");
    GtkWidget *applyButton = gtk_button_new_with_label("Apply");
    gtk_grid_attach(GTK_GRID(grid), applyAndHideButton, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), applyButton, 1, 5, 1, 1);

    // Set entries array
    auto **entries = new GtkEntry *[5]{
        GTK_ENTRY(deviceEntry),
        GTK_ENTRY(buttonEntry),
        GTK_ENTRY(pttOnEntry),
        GTK_ENTRY(pttOffEntry),
        GTK_ENTRY(volumeEntry)
    };

    auto *applyAndQuitData = new ButtonData{entries, true, settingsWindow};
    auto *applyOnlyData = new ButtonData{entries, false, settingsWindow};

    // Connect signals
    g_signal_connect(applyAndHideButton, "clicked", G_CALLBACK(onApplyButtonClicked), applyAndQuitData);
    g_signal_connect(applyButton, "clicked", G_CALLBACK(onApplyButtonClicked), applyOnlyData);
    g_signal_connect(settingsWindow, "destroy", G_CALLBACK(onWindowDestroy), applyAndQuitData);

    gtk_widget_show_all(settingsWindow);
}
