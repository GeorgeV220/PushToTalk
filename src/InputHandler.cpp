#include "InputHandler.h"

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#include "Utility.h"

#define settings Utility::settings

InputHandler::InputHandler() : fd(-1) {
    settings.loadSettings();
}

InputHandler::~InputHandler() {
    if (fd != -1) {
        close(fd);
    }
}

bool InputHandler::initializeDevice() {
    fd = open(settings.sDevice.c_str(), O_RDONLY);
    if (fd == -1) {
        Utility::error("Failed to open input device: " + settings.sDevice);
        return false;
    }

    Utility::print("Input device opened: " + settings.sDevice);
    return true;
}

void InputHandler::processEvents() const {
    input_event event{};
    bool isButton1Pressed = false;
    bool isButton2Pressed = false;
    int previousButton1State = 0;
    int previousButton2State = 0;

    while (true) {
        if (const ssize_t bytesRead = read(fd, &event, sizeof(input_event)); bytesRead == -1) {
            Utility::pError("Error reading input event");
            return;
        }

        if (event.type == EV_KEY) {
            if (event.code == settings.sButton) {
                isButton1Pressed = event.value;
                if (previousButton1State != isButton1Pressed) {
                    previousButton1State = isButton1Pressed;
                    Utility::debugPrint("Button 1 " + std::string(isButton1Pressed ? "pressed" : "released"));
                }
            } else if (event.code == settings.sButton2) {
                isButton2Pressed = event.value;
                if (previousButton2State != isButton2Pressed) {
                    Utility::debugPrint("Button 2 " + std::string(isButton2Pressed ? "pressed" : "released"));
                    previousButton2State = isButton2Pressed;

                    Utility::playSound((!isButton2Pressed ? settings.sPttOffPath : settings.sPttOnPath).c_str());
                    Utility::setMicMute(!isButton2Pressed);
                }
            }

            if (isButton1Pressed && isButton2Pressed) {
                Utility::print("Both buttons pressed. Shutting down.");
                Utility::setMicMute(true);
                break;
            }
        }
    }
}
