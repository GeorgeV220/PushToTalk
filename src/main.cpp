#include "InputHandler.h"
#include "Utility.h"
#include "gui/SettingsGUI.h"


int main(const int argc, char *argv[]) {
    InputHandler handler;

    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "--debug") {
            Utility::debug = true;
        } else if (arg == "--gui") {
            SettingsGUI::showSettingsGui(handler.getSettings());
            return 0;
        }
    }


    if (!handler.initializeDevice()) {
        return 1;
    }


    handler.processEvents();
    return 0;
}
