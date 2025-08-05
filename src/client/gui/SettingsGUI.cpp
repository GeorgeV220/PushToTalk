#include "SettingsGUI.h"

#include <algorithm>
#include <gtk/gtk.h>
#include <iostream>
#include <mutex>

#include "client/PushToTalkApp.h"
#include "client/utilities/Settings.h"
#include "common/utilities/Utility.h"
#include "common/utilities/numbers/Conversion.h"

std::mutex gtk_mutex;
GtkWidget *SettingsGUI::settingsWindow = nullptr;
GtkWidget *deviceBox = nullptr;
std::vector<std::pair<GtkWidget *, GtkWidget *> > deviceEntries;

void removeDeviceRow(GtkButton *button, gpointer user_data) {
    GtkWidget *row = GTK_WIDGET(user_data);

    // Find and erase the entry from deviceEntries
    const auto it = std::ranges::remove_if(deviceEntries,
                                           [row](const std::pair<GtkWidget *, GtkWidget *> &entry) {
                                               return gtk_widget_get_parent(entry.first) == row;
                                           }).begin();
    deviceEntries.erase(it, deviceEntries.end());

    gtk_container_remove(GTK_CONTAINER(deviceBox), row);
}

void onAddDeviceClicked(GtkButton *, gpointer) {
    GtkWidget *deviceEntry = gtk_entry_new();
    GtkWidget *buttonEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(deviceEntry), "vendor:product:uid");
    gtk_entry_set_placeholder_text(GTK_ENTRY(buttonEntry), "button");

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    GtkWidget *removeBtn = gtk_button_new_with_label("X");
    g_signal_connect(removeBtn, "clicked", G_CALLBACK(removeDeviceRow), row);

    gtk_box_pack_start(GTK_BOX(row), deviceEntry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), buttonEntry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), removeBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deviceBox), row, FALSE, FALSE, 0);

    deviceEntries.emplace_back(deviceEntry, buttonEntry);
    gtk_widget_show_all(deviceBox);
}

// ReSharper disable once CppDFAConstantFunctionResult
gboolean onSaveSettings(gpointer) {
    std::lock_guard lock(gtk_mutex);

    std::vector<DeviceSettings> devices;
    for (const auto &[deviceEntry, buttonEntry]: deviceEntries) {
        const std::string deviceStr = gtk_entry_get_text(GTK_ENTRY(deviceEntry));
        const IntConversionResult buttonRes = safeStrToInt(gtk_entry_get_text(GTK_ENTRY(buttonEntry)));
        if (!buttonRes.success) {
            Utility::error("Invalid button value.");
            return G_SOURCE_REMOVE;
        }
        devices.push_back({deviceStr, buttonRes.value});
    }

    const char *onPath = gtk_entry_get_text(
            GTK_ENTRY(g_object_get_data(G_OBJECT(SettingsGUI::settingsWindow), "pttOn")));
    const char *offPath = gtk_entry_get_text(
            GTK_ENTRY(g_object_get_data(G_OBJECT(SettingsGUI::settingsWindow), "pttOff")));
    const char *volumeStr = gtk_entry_get_text(
            GTK_ENTRY(g_object_get_data(G_OBJECT(SettingsGUI::settingsWindow), "volume")));
    const char *rateStr = gtk_entry_get_text(
            GTK_ENTRY(g_object_get_data(G_OBJECT(SettingsGUI::settingsWindow), "rate")));
    const char *channelsStr = gtk_entry_get_text(
            GTK_ENTRY(g_object_get_data(G_OBJECT(SettingsGUI::settingsWindow), "channels")));
    const char *bufferFramesStr = gtk_entry_get_text(
            GTK_ENTRY(g_object_get_data(G_OBJECT(SettingsGUI::settingsWindow), "bufferFrames")));

    float volume;
    try {
        const FloatConversionResult volRes = safeStrToFloat(volumeStr);
        if (!volRes.success) {
            Utility::error("Invalid volume value.");
            return G_SOURCE_REMOVE;
        }
        volume = volRes.value;
    } catch (...) {
        Utility::error("Invalid volume value.");
        return G_SOURCE_REMOVE;
    }
    int rate;
    try {
        const IntConversionResult rateRes = safeStrToInt(rateStr);
        if (!rateRes.success) {
            Utility::error("Invalid rate value.");
            return G_SOURCE_REMOVE;
        }
        rate = rateRes.value;
    } catch (...) {
        Utility::error("Invalid rate value.");
        return G_SOURCE_REMOVE;
    }
    int channels;
    try {
        const IntConversionResult channelsRes = safeStrToInt(channelsStr);
        if (!channelsRes.success) {
            Utility::error("Invalid channels value.");
            return G_SOURCE_REMOVE;
        }
        channels = channelsRes.value;
    } catch (...) {
        Utility::error("Invalid channels value.");
        return G_SOURCE_REMOVE;
    }
    int buffer_frames;
    try {
        const IntConversionResult bufferFrameRes = safeStrToInt(bufferFramesStr);
        if (!bufferFrameRes.success) {
            Utility::error("Invalid buffer size value.");
            return G_SOURCE_REMOVE;
        }
        buffer_frames = bufferFrameRes.value;
    } catch (...) {
        Utility::error("Invalid buffer size value.");
        return G_SOURCE_REMOVE;
    }

    Settings::settings.devices = std::move(devices);
    Settings::settings.sPttOnPath = onPath;
    Settings::settings.sPttOffPath = offPath;
    Settings::settings.sVolume = volume;
    Settings::settings.rate = rate;
    Settings::settings.channels = channels;
    Settings::settings.buffer_frames = buffer_frames;
    Settings::settings.saveSettings();

    InputClient &client = PushToTalkApp::getInstance().getClient();
    client.clear_devices();
    for (const auto &dev: Settings::settings.devices) {
        client.add_device(dev.getVendorID(), dev.getProductID(), dev.getDeviceUID(), dev.button);
    }

    client.restart();

    VirtualMicrophone &microphone = PushToTalkApp::getInstance().getVirtualMicrophone();
    microphone.set_audio_config(rate, channels, buffer_frames);
    microphone.restart();

    gtk_widget_hide(SettingsGUI::settingsWindow);
    return G_SOURCE_REMOVE;
}


void SettingsGUI::showSettingsGui() {
    std::lock_guard lock(gtk_mutex);
    if (settingsWindow) {
        gtk_window_present(GTK_WINDOW(settingsWindow));
        return;
    }

    settingsWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(settingsWindow), "PTT Settings");
    gtk_window_set_default_size(GTK_WINDOW(settingsWindow), 400, 700);

    GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(settingsWindow), 10);
    gtk_container_add(GTK_CONTAINER(settingsWindow), mainBox);

    GtkWidget *scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(mainBox), scroll, TRUE, TRUE, 0);

    deviceBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(scroll), deviceBox);

    for (const auto &[deviceStr, button]: Settings::settings.devices) {
        GtkWidget *deviceEntry = gtk_entry_new();
        GtkWidget *buttonEntry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(deviceEntry), deviceStr.c_str());
        gtk_entry_set_text(GTK_ENTRY(buttonEntry), std::to_string(button).c_str());

        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

        GtkWidget *removeBtn = gtk_button_new_with_label("X");
        g_signal_connect(removeBtn, "clicked", G_CALLBACK(removeDeviceRow), row);

        gtk_box_pack_start(GTK_BOX(row), deviceEntry, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), buttonEntry, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), removeBtn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(deviceBox), row, FALSE, FALSE, 0);

        deviceEntries.emplace_back(deviceEntry, buttonEntry);
    }

    GtkWidget *addDeviceBtn = gtk_button_new_with_label("+ Add Device");
    gtk_box_pack_start(GTK_BOX(mainBox), addDeviceBtn, FALSE, FALSE, 0);
    g_signal_connect(addDeviceBtn, "clicked", G_CALLBACK(onAddDeviceClicked), NULL);

    GtkWidget *pttOn = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(pttOn), Settings::settings.sPttOnPath.c_str());
    g_object_set_data(G_OBJECT(settingsWindow), "pttOn", pttOn);

    GtkWidget *pttOff = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(pttOff), Settings::settings.sPttOffPath.c_str());
    g_object_set_data(G_OBJECT(settingsWindow), "pttOff", pttOff);

    GtkWidget *volumeEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(volumeEntry), std::to_string(Settings::settings.sVolume).c_str());
    g_object_set_data(G_OBJECT(settingsWindow), "volume", volumeEntry);

    GtkWidget *rateEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(rateEntry), std::to_string(Settings::settings.rate).c_str());
    g_object_set_data(G_OBJECT(settingsWindow), "rate", rateEntry);

    GtkWidget *channelsEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(channelsEntry), std::to_string(Settings::settings.channels).c_str());
    g_object_set_data(G_OBJECT(settingsWindow), "channels", channelsEntry);

    GtkWidget *bufferFramesEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(bufferFramesEntry), std::to_string(Settings::settings.buffer_frames).c_str());
    g_object_set_data(G_OBJECT(settingsWindow), "bufferFrames", bufferFramesEntry);

    gtk_box_pack_start(GTK_BOX(mainBox), gtk_label_new("PTT On Path:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), pttOn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), gtk_label_new("PTT Off Path:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), pttOff, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), gtk_label_new("Volume:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), volumeEntry, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(mainBox), gtk_label_new("Rate:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), rateEntry, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(mainBox), gtk_label_new("Channels:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), channelsEntry, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(mainBox), gtk_label_new("Buffer Frames:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), bufferFramesEntry, FALSE, FALSE, 0);

    GtkWidget *saveBtn = gtk_button_new_with_label("Save & Close");
    gtk_box_pack_start(GTK_BOX(mainBox), saveBtn, FALSE, FALSE, 0);
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(+[](GtkButton *, gpointer) {
        gdk_threads_add_idle(onSaveSettings, nullptr);
    }), NULL);

    g_signal_connect(settingsWindow, "destroy", G_CALLBACK(+[](GtkWidget *, gpointer) {
        settingsWindow = nullptr;
        deviceEntries.clear();
    }), NULL);

    gtk_widget_show_all(settingsWindow);
}
