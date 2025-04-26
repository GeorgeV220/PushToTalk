#include "CommandLine.h"

#include "common/utilities/Utility.h"
#include "server/device/VirtualInputProxy.h"

#include <string>
#include <unordered_map>
#include <functional>

void CommandLine::handle(const int argc, char *argv[]) {
    const std::unordered_map<std::string, std::function<void()> > commands = {
        {"--debug", [] { Utility::set_debug(true); }},
        {"--detect", [] { VirtualInputProxy::detect_devices(); }}
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (auto it = commands.find(arg); it != commands.end()) {
            it->second();
        }
    }
}
