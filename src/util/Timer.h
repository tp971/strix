#pragma once

#include <iostream>
#include <iomanip>

class Timer {
  private:
    const bool timing;
    std::string section;
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point stop_time;

  public:
    Timer(bool timing = true) : timing(timing) {};
    ~Timer() {};

    inline void start(const std::string section_name = "") {
        section = std::move(section_name);
        if (timing) {
            std::cout << " * Starting " << section << " …" << std::endl;
            start_time = std::chrono::high_resolution_clock::now();
        }
    }
    inline void stop() {
        if (timing) {
            stop_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> dt = stop_time - start_time;
            std::cout << " * Finished " << section << ", took " << std::fixed << std::setprecision(2) << dt.count() << " seconds." << std::endl;
        }
    }
};
