#pragma once

#include <chrono>

class Clock {
    using clock = std::chrono::steady_clock;
    clock::time_point start = clock::now();

public:
    void restart();

    float getElapsedTime() const;
};