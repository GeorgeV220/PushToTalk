#include "InputHandler.h"

#include <cstdint>
#include <cstring>

#include "Utility.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

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
    input_event ie{};
    bool buttonPressed = false;
    bool button2Pressed = false;

    while (true) {
        if (const ssize_t bytes = read(fd, &ie, sizeof(struct input_event)); bytes == -1) {
            Utility::pError("Read error");
            return;
        }

        if (ie.type == EV_KEY) {
            Utility::debugPrint("Key pressed: " + std::to_string(ie.code));
            if (ie.code == settings.sButton) {
                Utility::debugPrint("Button " + std::to_string(ie.code) + (ie.value ? " pressed" : " released"));
                buttonPressed = ie.value;
            } else if (ie.code == settings.sButton2) {
                Utility::debugPrint("Button " + std::to_string(ie.code) + (ie.value ? " pressed" : " released"));
                button2Pressed = ie.value;
                Utility::setMicMute(!button2Pressed);
            }

            if (buttonPressed && button2Pressed) {
                Utility::print("Both buttons pressed, shutting down.");
                Utility::setMicMute(true);
                break;
            }
        }
    }
}