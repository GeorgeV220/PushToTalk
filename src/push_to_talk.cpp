#include "push_to_talk.h"

#include "VirtualInputProxy.cpp"
#include "Utility.h"

PushToTalk::PushToTalk() { Utility::initAudioSystem(); }
PushToTalk::~PushToTalk() { Utility::cleanupAudioSystem(); }

int PushToTalk::run() {
    try {
        VirtualInputProxy proxy(Settings::settings.sDevice, Settings::settings.sButton2);

        proxy.set_callback([](const bool pressed) {
            Utility::debugPrint("Button 2 " + std::string(pressed ? "pressed" : "released"));

            Utility::playSound((!pressed ? Settings::settings.sPttOffPath : Settings::settings.sPttOnPath).c_str());
            Utility::setMicMute(!pressed);
        });

        proxy.start();

        while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (const std::exception &e) {
        Utility::error("Error: " + std::string(e.what()));
        return 1;
    }
}
