#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <mutex>
#include <iostream>
#include <algorithm>
#include <type_traits>

#include "SettingsGUI.h"
#include "client/PushToTalkApp.h"
#include "client/utilities/Settings.h"
#include "common/utilities/Utility.h"
#include "common/utilities/numbers/Conversion.h"

std::mutex gtk_mutex;
GtkWidget *SettingsGUI::settingsWindow = nullptr;
GtkWidget *deviceBox = nullptr;

struct DeviceRow {
    GtkWidget *deviceEntry;
    GtkWidget *buttonEntry;
    GtkWidget *exclusiveCheck;
};

std::vector<DeviceRow> deviceEntries;

void removeDeviceRow([[maybe_unused]] GtkButton *button, gpointer user_data) {
    GtkWidget *row = GTK_WIDGET(user_data);
    const auto it = std::ranges::remove_if(deviceEntries, [row](const auto &entry) {
        return gtk_widget_get_parent(entry.deviceEntry) == row;
    }).begin();
    deviceEntries.erase(it, deviceEntries.end());
    gtk_container_remove(GTK_CONTAINER(deviceBox), row);
}

void onAddDeviceClicked(GtkButton *, gpointer) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *deviceEntry = gtk_entry_new();
    GtkWidget *buttonEntry = gtk_entry_new();
    GtkWidget *exclusiveCheck = gtk_check_button_new_with_label("Exclusive");

    gtk_entry_set_placeholder_text(GTK_ENTRY(deviceEntry), "vendor:product:uid");
    gtk_entry_set_placeholder_text(GTK_ENTRY(buttonEntry), "button");

    GtkWidget *removeBtn = gtk_button_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(removeBtn, "Remove this device");
    g_signal_connect(removeBtn, "clicked", G_CALLBACK(removeDeviceRow), row);

    gtk_box_pack_start(GTK_BOX(row), deviceEntry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), buttonEntry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), exclusiveCheck, FALSE, FALSE, 0); // add checkbox
    gtk_box_pack_start(GTK_BOX(row), removeBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deviceBox), row, FALSE, FALSE, 0);

    deviceEntries.push_back({deviceEntry, buttonEntry, exclusiveCheck});
    gtk_widget_show_all(deviceBox);
}

void add_grid_entry(GtkGrid *grid, const char *label, const char *value, int row, const char *key,
                    GtkWidget *parent_window) {
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), value);

    gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);

    if (key && parent_window) {
        g_object_set_data(G_OBJECT(parent_window), key, entry);
    }
}

template<typename T>
void add_grid_slider_entry(GtkGrid *grid, const char *label, T value, double step, int min, int max, int row,
                           const char *key,
                           GtkWidget *parent_window) {
    static_assert(std::is_arithmetic<T>::value, "T must be a numeric type");

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);

    GtkWidget *entry = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    gtk_range_set_value(GTK_RANGE(entry), static_cast<double>(value));

    gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);

    if (key && parent_window) {
        g_object_set_data(G_OBJECT(parent_window), key, entry);
    }
}

gboolean onSaveSettings(gpointer) {
    std::lock_guard lock(gtk_mutex);
    std::vector<DeviceSettings> devices;

    for (const auto &[deviceEntry, buttonEntry, exclusiveCheck]: deviceEntries) {
        const std::string deviceStr = gtk_entry_get_text(GTK_ENTRY(deviceEntry));
        const IntConversionResult buttonRes = safeStrToInt(gtk_entry_get_text(GTK_ENTRY(buttonEntry)));
        if (!buttonRes.success) {
            Utility::error("Invalid button value.");
            return G_SOURCE_REMOVE;
        }

        devices.push_back(DeviceSettings{
            deviceStr,
            buttonRes.value,
            static_cast<bool>(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(exclusiveCheck)))
        });
    }

    auto get_entry_text = [](GtkWidget *window, const char *key) {
        return gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(window), key)));
    };

    auto get_scale_value = [](GtkWidget *window, const char *key) {
        return static_cast<float>(gtk_range_get_value(GTK_RANGE(g_object_get_data(G_OBJECT(window), key))));
    };

    try {
        Settings::settings.devices = std::move(devices);
        Settings::settings.sPttOnPath = get_entry_text(SettingsGUI::settingsWindow, "pttOn");
        Settings::settings.sPttOffPath = get_entry_text(SettingsGUI::settingsWindow, "pttOff");
        Settings::settings.sVolume = static_cast<float>(get_scale_value(SettingsGUI::settingsWindow, "volume"));
        Settings::settings.rate = safeStrToInt(get_entry_text(SettingsGUI::settingsWindow, "rate")).value;
        Settings::settings.channels = safeStrToInt(get_entry_text(SettingsGUI::settingsWindow, "channels")).value;
        Settings::settings.buffer_frames = safeStrToInt(
            get_entry_text(SettingsGUI::settingsWindow, "bufferFrames")).value;
        Settings::settings.capture_buffer_size = safeStrToInt(
            get_entry_text(SettingsGUI::settingsWindow, "captureBufferSize")).value;
        Settings::settings.playback_buffer_size = safeStrToInt(
            get_entry_text(SettingsGUI::settingsWindow, "playbackBufferSize")).value;
    } catch (...) {
        Utility::error("One or more fields have invalid values.");
        return G_SOURCE_REMOVE;
    }

    Settings::settings.saveSettings();
    PushToTalkApp::getInstance().reload();

    return G_SOURCE_REMOVE;
}

void SettingsGUI::showSettingsGui() {
    std::lock_guard lock(gtk_mutex);

    if (settingsWindow) {
        gtk_window_present(GTK_WINDOW(settingsWindow));
        return;
    }

    settingsWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(settingsWindow), "Push-To-Talk Settings");
    gtk_window_set_default_size(GTK_WINDOW(settingsWindow), 600, 500);
    gtk_container_set_border_width(GTK_CONTAINER(settingsWindow), 10);


    GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(settingsWindow), mainBox);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(mainBox), notebook, TRUE, TRUE, 0);


    GtkWidget *deviceTab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(deviceTab), 10);
    deviceBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(deviceTab), deviceBox, TRUE, TRUE, 0);

    for (const auto &[deviceStr, button, exclusive]: Settings::settings.devices) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget *deviceEntry = gtk_entry_new();
        GtkWidget *buttonEntry = gtk_entry_new();
        GtkWidget *exclusiveCheck = gtk_check_button_new_with_label("Exclusive");

        gtk_entry_set_text(GTK_ENTRY(deviceEntry), deviceStr.c_str());
        gtk_entry_set_text(GTK_ENTRY(buttonEntry), std::to_string(button).c_str());
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(exclusiveCheck), exclusive);

        GtkWidget *removeBtn = gtk_button_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_tooltip_text(removeBtn, "Remove this device");
        g_signal_connect(removeBtn, "clicked", G_CALLBACK(removeDeviceRow), row);

        gtk_box_pack_start(GTK_BOX(row), deviceEntry, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), buttonEntry, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), exclusiveCheck, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), removeBtn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(deviceBox), row, FALSE, FALSE, 0);

        deviceEntries.push_back({deviceEntry, buttonEntry, exclusiveCheck});
    }

    GtkWidget *addDeviceBtn = gtk_button_new_with_label("➕ Add Device");
    gtk_box_pack_start(GTK_BOX(deviceTab), addDeviceBtn, FALSE, FALSE, 5);
    g_signal_connect(addDeviceBtn, "clicked", G_CALLBACK(onAddDeviceClicked), NULL);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), deviceTab, gtk_label_new("Devices"));


    GtkWidget *audioTab = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(audioTab), 10);
    gtk_grid_set_column_spacing(GTK_GRID(audioTab), 10);
    gtk_container_set_border_width(GTK_CONTAINER(audioTab), 10);

    add_grid_entry(GTK_GRID(audioTab), "PTT On Path:", Settings::settings.sPttOnPath.c_str(), 0, "pttOn",
                   settingsWindow);
    add_grid_entry(GTK_GRID(audioTab), "PTT Off Path:", Settings::settings.sPttOffPath.c_str(), 1, "pttOff",
                   settingsWindow);
    add_grid_slider_entry(GTK_GRID(audioTab), "Volume:", Settings::settings.sVolume, 0.01, 0, 1, 2, "volume",
                          settingsWindow);
    add_grid_entry(GTK_GRID(audioTab), "Rate:", std::to_string(Settings::settings.rate).c_str(), 3, "rate",
                   settingsWindow);
    add_grid_entry(GTK_GRID(audioTab), "Channels:", std::to_string(Settings::settings.channels).c_str(), 4, "channels",
                   settingsWindow);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), audioTab, gtk_label_new("Audio"));


    GtkWidget *bufferTab = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(bufferTab), 10);
    gtk_grid_set_column_spacing(GTK_GRID(bufferTab), 10);
    gtk_container_set_border_width(GTK_CONTAINER(bufferTab), 10);

    add_grid_entry(GTK_GRID(bufferTab), "Buffer Frames:", std::to_string(Settings::settings.buffer_frames).c_str(), 0,
                   "bufferFrames", settingsWindow);
    add_grid_entry(GTK_GRID(bufferTab), "Capture Buffer Size:",
                   std::to_string(Settings::settings.capture_buffer_size).c_str(), 1, "captureBufferSize",
                   settingsWindow);
    add_grid_entry(GTK_GRID(bufferTab), "Playback Buffer Size:",
                   std::to_string(Settings::settings.playback_buffer_size).c_str(), 2, "playbackBufferSize",
                   settingsWindow);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), bufferTab, gtk_label_new("Buffer"));


    GtkWidget *buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(buttonBox), FALSE);

    GtkWidget *saveBtn = gtk_button_new_with_label("💾 Save");
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(+[](GtkButton *, gpointer) {
                         gdk_threads_add_idle(onSaveSettings, nullptr);
                         }), NULL);

    GtkWidget *saveCloseBtn = gtk_button_new_with_label("💾 Save and Close");
    g_signal_connect(saveCloseBtn, "clicked", G_CALLBACK(+[](GtkButton *, gpointer) {
                         onSaveSettings(nullptr);
                         gtk_widget_hide(SettingsGUI::settingsWindow);
                         return FALSE;
                         }), NULL);

    gtk_box_pack_end(GTK_BOX(buttonBox), saveCloseBtn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(buttonBox), saveBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), buttonBox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deviceTab), buttonBox, FALSE, FALSE, 5);


    g_signal_connect(settingsWindow, "destroy", G_CALLBACK(+[](GtkWidget *, gpointer) {
                         settingsWindow = nullptr;
                         deviceEntries.clear();
                         }), NULL);

    gtk_widget_show_all(settingsWindow);
}
