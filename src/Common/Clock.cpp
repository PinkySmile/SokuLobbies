#include <Clock.hpp>

void Clock::restart() {
    start = clock::now();
}

float Clock::getElapsedTime() const {
    return std::chrono::duration<float>(
            clock::now() - start
        ).count();
}