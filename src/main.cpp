#include "push_to_talk.h"
#include "Settings.h"
#include "Utility.h"
#include "gui/SettingsGUI.h"


int main(const int argc, char *argv[]) {
    Settings::settings.loadSettings();
    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "--debug") {
            Utility::debug = true;
        } else if (arg == "--gui") {
            SettingsGUI::showSettingsGui();
            return 0;
        } else if (arg == "--detect") {
            PushToTalk::detect_devices();
            return 0;
        }
    }

    PushToTalk app;
    return app.run();
}
