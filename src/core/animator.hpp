#pragma once
#include "core/package.hpp"
#include <optional>
namespace pet {
struct Advance {
    size_t frame = 0;
    std::vector<Marker> markers;
    bool finished = false, finishedEvent = false;
    uint64_t skippedCycles = 0;
    std::optional<int64_t> nextDeadlineUs;
};
class Animator {
    const std::vector<Frame>* frames_ = nullptr;
    uint64_t duration_ = 0, lastTick_ = 0;
    uint32_t hz_ = 0, numerator_ = 1, denominator_ = 1;
    int64_t startUs_ = 0, pausedUs_ = 0, lastNow_ = 0;
    bool loop_ = true, sampled_ = false, paused_ = false, finishedSent_ = false;
public:
    void start(const Package&, uint32_t clipId, unsigned direction, bool loop, int64_t nowUs, std::uint32_t durationScale=1);
    void changeDirection(const Package&, uint32_t clipId, unsigned direction);
    bool tryChangeDirection(const Package&, uint32_t clipId, unsigned direction);
    Advance advance(int64_t nowUs);
    void pause(int64_t nowUs);
    void resume(int64_t nowUs);
    void stop() { frames_ = nullptr; }
    bool active() const { return frames_ != nullptr; }
};
}
