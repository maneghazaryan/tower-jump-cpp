#pragma once
// Fixed-timestep accumulator ("Fix Your Timestep"). The simulation always advances in equal steps
// no matter the display rate, so physics and jump heights are identical at 30, 60 or 144 Hz.
// advance() returns the interpolation factor between the last two simulated states.

#include <algorithm>

namespace tj {

class FixedTimestep {
public:
    explicit FixedTimestep(float stepSeconds = 1.0f / 120.0f, int maxStepsPerFrame = 8)
        : step_(stepSeconds), maxSteps_(maxStepsPerFrame) {}

    template <typename StepFn>
    float advance(float frameSeconds, StepFn&& stepFn) {
        // Clamp huge frame times (tab in background, debugger pause) instead of spiralling.
        accumulator_ += std::min(frameSeconds, step_ * static_cast<float>(maxSteps_));
        int steps = 0;
        while (accumulator_ >= step_ && steps < maxSteps_) {
            stepFn(step_);
            accumulator_ -= step_;
            ++steps;
        }
        lastSteps_ = steps;
        return accumulator_ / step_;
    }

    float step() const { return step_; }
    int lastSteps() const { return lastSteps_; }
    void reset() { accumulator_ = 0; }

private:
    float step_;
    int maxSteps_;
    float accumulator_ = 0;
    int lastSteps_ = 0;
};

}  // namespace tj
