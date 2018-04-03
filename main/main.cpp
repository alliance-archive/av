#include <chrono>
#include <csignal>
#include <thread>

#include "lib/server.hpp"

namespace {
    volatile std::sig_atomic_t gSignal = 0;
}

void signalHandler(int signal) {
    gSignal = signal;
}

int main(int argc, const char* argv[]) {
    std::signal(SIGINT, signalHandler);

    Server s;
    s.start();

    while (gSignal != SIGINT) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    s.stop();

    return 0;
}
