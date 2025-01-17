#include "SettingsGUI.h"
#include <fcntl.h>
#include <gtk/gtk.h>
#include <iostream>
#include <linux/input.h>

#include "../Settings.h"
#include "../Utility.h"

struct ButtonData {
    GtkEntry **entries;
    bool quit;
};

void onApplyButtonClicked(const GtkButton *button, const gpointer data) {
    (void) button; // Explicitly ignore unused parameter

    const auto *buttonData = static_cast<ButtonData *>(data);
    GtkEntry **entries = buttonData->entries;
    const bool quit = buttonData->quit;

    const char *device = gtk_entry_get_text(entries[0]);
    const ConversionResult button1Result = Settings::safeAtoi(gtk_entry_get_text(entries[1]));
    const ConversionResult button2Result = Settings::safeAtoi(gtk_entry_get_text(entries[2]));

    if (button1Result.success && button2Result.success) {
        Settings settings;
        settings.saveSettings(
            device,
            button1Result.value,
            button2Result.value
        );
        Utility::print("Settings saved.");
        if (quit) {
            gtk_main_quit();
        }
    } else {
        Utility::error("Invalid button values.");
    }
}

void onQuitButtonClicked(const GtkButton *button) {
    (void) button; // Explicitly ignore unused parameter
    gtk_main_quit();
}

void SettingsGUI::showSettingsGui(const Settings &settings) {
    GtkEntry *entries[5];
    gtk_init(nullptr, nullptr);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Settings");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_container_add(GTK_CONTAINER(window), grid);

    GtkWidget *deviceLabel = gtk_label_new("Device:");
    GtkWidget *deviceEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(deviceEntry), settings.sDevice.c_str());

    gtk_widget_set_hexpand(deviceEntry, TRUE);
    gtk_widget_set_vexpand(deviceEntry, FALSE);

    gtk_grid_attach(GTK_GRID(grid), deviceLabel, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), deviceEntry, 1, 0, 1, 1);

    GtkWidget *deviceInfoButton = gtk_button_new_with_label("!");
    gtk_widget_set_tooltip_text(deviceInfoButton, "Enter the path to your input device (e.g., /dev/input/event5)");
    gtk_grid_attach(GTK_GRID(grid), deviceInfoButton, 2, 0, 1, 1);

    GtkWidget *button1Label = gtk_label_new("Button 1:");
    GtkWidget *button1Entry = gtk_entry_new();
    const std::string button1Text = std::to_string(settings.sButton);
    gtk_entry_set_text(GTK_ENTRY(button1Entry), button1Text.c_str());

    gtk_widget_set_hexpand(button1Entry, TRUE);
    gtk_widget_set_vexpand(button1Entry, FALSE);

    gtk_grid_attach(GTK_GRID(grid), button1Label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), button1Entry, 1, 1, 1, 1);

    GtkWidget *button1InfoButton = gtk_button_new_with_label("!");
    gtk_widget_set_tooltip_text(button1InfoButton,
                                "Enter the button code for the first button (e.g., 276 for BTN_EXTRA)");
    gtk_grid_attach(GTK_GRID(grid), button1InfoButton, 2, 1, 1, 1);

    GtkWidget *button2Label = gtk_label_new("Button 2:");
    GtkWidget *button2Entry = gtk_entry_new();
    const std::string button2Text = std::to_string(settings.sButton2);
    gtk_entry_set_text(GTK_ENTRY(button2Entry), button2Text.c_str());

    gtk_widget_set_hexpand(button2Entry, TRUE);
    gtk_widget_set_vexpand(button2Entry, FALSE);

    gtk_grid_attach(GTK_GRID(grid), button2Label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), button2Entry, 1, 2, 1, 1);

    GtkWidget *button2InfoButton = gtk_button_new_with_label("!");
    gtk_widget_set_tooltip_text(button2InfoButton,
                                "Enter the button code for the second button (e.g., 275 for BTN_SIDE)");
    gtk_grid_attach(GTK_GRID(grid), button2InfoButton, 2, 2, 1, 1);

    GtkWidget *applyAndQuitButton = gtk_button_new_with_label("Apply and Quit");
    GtkWidget *applyButton = gtk_button_new_with_label("Apply");
    GtkWidget *quitButton = gtk_button_new_with_label("Quit");

    gtk_widget_set_hexpand(applyAndQuitButton, TRUE);
    gtk_widget_set_vexpand(applyAndQuitButton, FALSE);
    gtk_widget_set_hexpand(applyButton, TRUE);
    gtk_widget_set_vexpand(applyButton, FALSE);
    gtk_widget_set_hexpand(quitButton, TRUE);
    gtk_widget_set_vexpand(quitButton, FALSE);

    gtk_grid_attach(GTK_GRID(grid), applyAndQuitButton, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), applyButton, 1, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), quitButton, 2, 5, 1, 1);

    entries[0] = GTK_ENTRY(deviceEntry);
    entries[1] = GTK_ENTRY(button1Entry);
    entries[2] = GTK_ENTRY(button2Entry);

    auto *applyAndQuitData = new ButtonData{entries, true};
    auto *applyOnlyData = new ButtonData{entries, false};

    g_signal_connect(applyAndQuitButton, "clicked", G_CALLBACK(onApplyButtonClicked), applyAndQuitData);
    g_signal_connect(applyButton, "clicked", G_CALLBACK(onApplyButtonClicked), applyOnlyData);
    g_signal_connect(quitButton, "clicked", G_CALLBACK(onQuitButtonClicked), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    gtk_widget_show_all(window);
    gtk_main();
}
