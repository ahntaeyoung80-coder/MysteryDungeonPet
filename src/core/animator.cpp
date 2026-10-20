#include "core/animator.hpp"
#include <algorithm>
#include <stdexcept>
namespace pet {
void Animator::start(const Package& p, uint32_t id, unsigned dir, bool loop, int64_t now, uint32_t durationScale) {
    if (now < 0) throw std::runtime_error("negative clock");
    if(!durationScale || durationScale>16)throw std::runtime_error("invalid playback duration scale");
    const auto it = std::find_if(p.clips.begin(), p.clips.end(), [id](const Clip& c) { return c.id == id; });
    if (it == p.clips.end()) throw std::runtime_error("unknown clip");
    const auto& c = *it;
    frames_ = &p.frames(id, dir); hz_ = p.tickHz; numerator_ = c.speedNumerator; denominator_ = c.speedDenominator*durationScale;
    duration_ = 0;
    for (const auto& f : *frames_) duration_ += f.ticks;
    startUs_ = lastNow_ = now; lastTick_ = 0; loop_ = loop; sampled_ = paused_ = finishedSent_ = false;
}
void Animator::changeDirection(const Package& p,uint32_t id,unsigned dir) {
    const auto& next=p.frames(id,dir);
    uint64_t duration=0; for(const auto& f:next) duration+=f.ticks;
    if(!frames_ || !loop_ || duration!=duration_) throw std::runtime_error("direction phase incompatible");
    frames_=&next; // Preserve start/pause/marker clocks; supported Walk directions share cycle duration.
}
Advance Animator::advance(int64_t now) {
    Advance out;
    if (!frames_) return out;
    if (now < lastNow_) throw std::runtime_error("non-monotonic clock");
    lastNow_ = now;
    const uint64_t elapsed = uint64_t((paused_ ? pausedUs_ : now) - startUs_);
    const uint64_t rate = uint64_t(hz_) * numerator_, unit = uint64_t(1000000) * denominator_;
    // Decomposed multiply avoids overflow for decades-long uptime.
    uint64_t tick = (elapsed / unit) * rate + (elapsed % unit) * rate / unit;
    if (!loop_) tick = std::min(tick, duration_);
    const uint64_t cycle = loop_ ? tick / duration_ : 0;
    const uint64_t position = loop_ ? tick % duration_ : std::min(tick, duration_ - 1);
    uint64_t emitFrom = sampled_ ? lastTick_ : 0;
    if (loop_ && sampled_ && cycle > lastTick_ / duration_ + 1) {
        out.skippedCycles = cycle - lastTick_ / duration_ - 1;
        emitFrom = cycle * duration_;
    }
    uint64_t offset = 0;
    for (size_t i = 0; i < frames_->size(); ++i) {
        const auto& f = (*frames_)[i];
        if (position >= offset && position < offset + f.ticks) out.frame = i;
        offset += f.ticks;
    }
    // Chronological marker order, bounded to at most two cycles.
    const uint64_t firstCycle = loop_ && cycle > 0 && emitFrom < cycle * duration_ ? cycle - 1 : cycle;
    for (uint64_t cy = firstCycle; cy <= cycle; ++cy) {
        offset = 0;
        for (const auto& f : *frames_) {
            const uint64_t boundary = cy * duration_ + offset;
            if ((!sampled_ || boundary > emitFrom || (out.skippedCycles && boundary == emitFrom)) && boundary <= tick)
                for (unsigned event = 0; event < 3; ++event)
                    if (f.events & (1U << event)) out.markers.push_back({event, f.anchor});
            offset += f.ticks;
        }
    }
    out.finished = !loop_ && tick == duration_;
    out.finishedEvent = out.finished && !finishedSent_;
    if (out.finished) finishedSent_ = true;
    if (!paused_ && !out.finished) {
        uint64_t boundary = cycle * duration_;
        for (const auto& f : *frames_) {
            boundary += f.ticks;
            if (boundary > tick) break;
        }
        const uint64_t deadline = (boundary / rate) * unit + ((boundary % rate) * unit + rate - 1) / rate;
        out.nextDeadlineUs = startUs_ + int64_t(deadline);
    }
    sampled_ = true; lastTick_ = tick;
    return out;
}
void Animator::pause(int64_t now) {
    if (!frames_ || paused_) return;
    if (now < lastNow_) throw std::runtime_error("non-monotonic pause");
    lastNow_ = now; pausedUs_ = now; paused_ = true;
}
void Animator::resume(int64_t now) {
    if (!frames_ || !paused_) return;
    if (now < lastNow_) throw std::runtime_error("non-monotonic resume");
    startUs_ += now - pausedUs_; lastNow_ = now; paused_ = false;
}
}
