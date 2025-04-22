#ifndef PUSH_TO_TALK_H
#define PUSH_TO_TALK_H

#include "InputClient.cpp"

class PushToTalk {
    InputClient client_;

public:
    PushToTalk();

    ~PushToTalk();

    int run();
};

#endif
