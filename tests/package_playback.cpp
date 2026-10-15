#include "core/animator.hpp"
#include "core/playback_sample.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
unsigned eventCount(uint32_t events) {
    return (events & 1U) + ((events >> 1) & 1U) + ((events >> 2) & 1U);
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected MDPA path");
        const auto p = pet::loadPackage(std::filesystem::u8path(argv[1]));
        unsigned selections = 0, framesChecked = 0;
        for (const auto& clip : p.clips) for (unsigned direction = 0; direction < 8; ++direction) {
            const auto& frames = p.frames(clip.id, direction);
            const uint64_t rate = uint64_t(p.tickHz) * clip.speedNumerator;
            const uint64_t unit = uint64_t(1000000) * clip.speedDenominator;
            auto timeFor = [&](uint64_t ticks) { return int64_t((ticks * unit + rate - 1) / rate); };
            pet::Animator a;
            constexpr int64_t base = 1000000;
            a.start(p, clip.id, direction, false, base);
            uint64_t ticks = 0;
            unsigned markers = 0, expectedMarkers = 0;
            for (size_t index = 0; index < frames.size(); ++index) {
                const auto now = base + timeFor(ticks);
                auto result = a.advance(now);
                require(result.frame == index && !result.finished, "frame/tick boundary mismatch");
                auto sample = pet::playbackSample(p, clip.id, direction, result.frame, a.active(), {33,33});
                require(sample.frame == &frames[index] && sample.frame->ticks == frames[index].ticks, "sample/Animator mismatch");
                require(a.advance(now).markers.empty(), "duplicate boundary marker");
                markers += unsigned(result.markers.size());
                expectedMarkers += eventCount(frames[index].events);
                ticks += frames[index].ticks;
                ++framesChecked;
            }
            auto end = a.advance(base + timeFor(ticks));
            require(end.finished && end.finishedEvent && end.frame == frames.size()-1 && !end.nextDeadlineUs, "finite completion mismatch");
            require(markers + end.markers.size() == expectedMarkers, "finite marker loss/duplication");
            auto repeat = a.advance(base + timeFor(ticks) + 1000000);
            require(!repeat.finishedEvent && repeat.markers.empty(), "duplicate finite completion");
            a.start(p, clip.id, direction, true, base); a.advance(base); a.pause(base);
            auto paused = a.advance(base + 5000000);
            require(paused.frame == 0 && paused.markers.empty() && !paused.nextDeadlineUs, "paused animation advanced");
            a.resume(base + 5000000);
            require(a.advance(base + 5000000).frame == 0, "resume changed logical time");
            auto delayed = a.advance(base + 5000000 + timeFor(ticks) * 100);
            require(!delayed.finished && delayed.skippedCycles > 1 && delayed.markers.size() <= expectedMarkers * 2,
                    "delayed loop replay is unbounded");
            a.stop(); require(!a.active() && !a.advance(base + 6000000 + timeFor(ticks) * 100).nextDeadlineUs, "explicit stop failed");
            ++selections;
        }
        std::cout << "{\"status\":\"pass\",\"selections\":" << selections << ",\"frameBoundaries\":" << framesChecked
                  << ",\"scope\":\"native tick/finite/marker/pause/delay only; original wall time and Windows pending\"}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
