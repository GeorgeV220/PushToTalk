#include "SettingsGUI.h"
#include <fcntl.h>
#include <gtk/gtk.h>
#include <iostream>
#include <linux/input.h>

#include "../Settings.h"
#include "../Utility.h"
#include "../utilities/numbers/Conversion.h"

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
    const IntConversionResult buttonResult = safeStrToInt(gtk_entry_get_text(entries[1]));
    const std::string pttOnPath = gtk_entry_get_text(entries[2]);
    const std::string pttOffPath = gtk_entry_get_text(entries[3]);
    const float volume = std::stof(gtk_entry_get_text(entries[4]));

    if (buttonResult.success) {
        Settings::settings.saveSettings(
            device,
            buttonResult.value,
            pttOnPath,
            pttOffPath,
            volume
        );
        Utility::print("Settings saved.");
        if (quit) {
            gtk_main_quit();
        }
    } else {
        Utility::error("Invalid button value.");
    }
}

void onQuitButtonClicked(const GtkButton *button) {
    (void) button; // Explicitly ignore unused parameter
    gtk_main_quit();
}

void SettingsGUI::showSettingsGui() {
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
    GtkWidget *applyAndQuitButton = gtk_button_new_with_label("Apply and Quit");
    GtkWidget *applyButton = gtk_button_new_with_label("Apply");
    GtkWidget *quitButton = gtk_button_new_with_label("Quit");
    gtk_grid_attach(GTK_GRID(grid), applyAndQuitButton, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), applyButton, 1, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), quitButton, 2, 5, 1, 1);

    // Set entries array
    entries[0] = GTK_ENTRY(deviceEntry);
    entries[1] = GTK_ENTRY(buttonEntry);
    entries[2] = GTK_ENTRY(pttOnEntry);
    entries[3] = GTK_ENTRY(pttOffEntry);
    entries[4] = GTK_ENTRY(volumeEntry);

    // Connect signals
    auto *applyAndQuitData = new ButtonData{entries, true};
    auto *applyOnlyData = new ButtonData{entries, false};
    g_signal_connect(applyAndQuitButton, "clicked", G_CALLBACK(onApplyButtonClicked), applyAndQuitData);
    g_signal_connect(applyButton, "clicked", G_CALLBACK(onApplyButtonClicked), applyOnlyData);
    g_signal_connect(quitButton, "clicked", G_CALLBACK(onQuitButtonClicked), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    gtk_widget_show_all(window);
    gtk_main();
}
