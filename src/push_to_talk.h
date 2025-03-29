#ifndef PUSH_TO_TALK_H
#define PUSH_TO_TALK_H
#include "Settings.h"
#include "device/VirtualInputProxy.h"

class PushToTalk {
    VirtualInputProxy proxy;

public:
    PushToTalk();

    ~PushToTalk();

    int run();

    static void detect_devices();
};

#endif
