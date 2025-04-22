#include "push_to_talk.h"

#include <iomanip>

#include "utilities/AudioUtilities.h"
#include "../common/Settings.h"
#include "../server/device/VirtualInputProxy.h"
#include "../common/utilities/Utility.h"
#include "../common/utilities/numbers/Conversion.h"

PushToTalk::PushToTalk()
    : client_(Settings::settings.getVendorID(), Settings::settings.getProductID(), Settings::settings.getDeviceUID(),
              Settings::settings.sButton) {
    AudioUtilities::initAudioSystem();
}

PushToTalk::~PushToTalk() { AudioUtilities::cleanupAudioSystem(); }

int PushToTalk::run() {
    try {
        client_.set_callback([](const bool pressed) {
            Utility::debugPrint(
                "Button " + safeIntToStr(Settings::settings.sButton).str + " " + std::string(
                    pressed ? "pressed" : "released"));
            AudioUtilities::playSound(
                (!pressed ? Settings::settings.sPttOffPath : Settings::settings.sPttOnPath).c_str());
            AudioUtilities::setMicMute(!pressed);
        });

        client_.start();
        while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (const std::exception &e) {
        Utility::error("Error: " + std::string(e.what()));
        return 1;
    }
}
