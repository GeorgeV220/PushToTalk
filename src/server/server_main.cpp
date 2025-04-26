#include <iostream>

#include "cli/CommandLine.h"
#include "InputProxyServer.h"

int main(const int argc, char *argv[]) {
    CommandLine::handle(argc, argv);

    try {
        InputProxyServer server;
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Server fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
