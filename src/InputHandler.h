#ifndef INPUTHANDLER_H
#define INPUTHANDLER_H

#include "Settings.h"

class InputHandler {
    int fd;

public:
    InputHandler();

    ~InputHandler();

    bool initializeDevice();

    void processEvents() const;
};

#endif // INPUTHANDLER_H
