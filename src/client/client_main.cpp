#include "PushToTalkApp.h"

int main(const int argc, char* argv[]) {
    auto& app = PushToTalkApp::getInstance(argc, argv);
    app.run();
    return 0;
}
