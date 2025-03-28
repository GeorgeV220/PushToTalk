#include "push_to_talk.h"

#include "InputHandler.h"
#include "Utility.h"

PushToTalk::PushToTalk() { Utility::initAudioSystem(); }
PushToTalk::~PushToTalk() { Utility::cleanupAudioSystem(); }

int PushToTalk::run() {
    InputHandler handler;

    if (!handler.initializeDevice()) {
        return -1;
    }

    handler.processEvents();
    return 0;
}
