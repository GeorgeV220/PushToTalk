#ifndef INPUTHANDLER_H
#define INPUTHANDLER_H

#include "Settings.h"

class InputHandler {
    Settings settings;
    int fd;

public:
    InputHandler();

    ~InputHandler();

    bool initializeDevice();

    void processEvents() const;

    Settings getSettings() const {
        return settings;
    }
};

#endif // INPUTHANDLER_H
