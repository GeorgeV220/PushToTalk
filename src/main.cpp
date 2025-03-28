#include "InputHandler.h"
#include "push_to_talk.h"
#include "Utility.h"
#include "gui/SettingsGUI.h"


int main(const int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "--debug") {
            Utility::debug = true;
        } else if (arg == "--gui") {
            SettingsGUI::showSettingsGui(Utility::settings);
            return 0;
        }
    }

    PushToTalk app;
    return app.run();
}
