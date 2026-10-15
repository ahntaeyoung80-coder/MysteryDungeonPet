#pragma once
#include "core/package.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
namespace pet {
constexpr int64_t kMoveDurationUs = 400000; // original normal movement: 24 ticks at 60 Hz
inline int64_t deadlineFromUtc(int64_t nowUs, uint64_t utcMs, uint64_t deadlineMs) {
    // Read UTC once; a deadline crossed between two reads must not underflow.
    return nowUs + int64_t(std::min<uint64_t>(60000, deadlineMs>utcMs?deadlineMs-utcMs:0))*1000;
}
struct WorldPoint { double x = 0, y = 0; };
inline WorldPoint tilePoint(int tile, int width = 24) { return {double(tile % width*24+12), double(tile/width*24+12)}; }
inline Point raster(WorldPoint p) { return {int32_t(std::lround(p.x)), int32_t(std::lround(p.y))}; }
inline Point cameraFor(WorldPoint p, int width, int height, int viewport) {
    const double anchor=viewport/2; // One integer anchor avoids alternating pixels at scale 3.
    return raster({std::clamp(p.x-anchor, 0.0, double(std::max(0,width-viewport))),
                   std::clamp(p.y-anchor, 0.0, double(std::max(0,height-viewport)))});
}
// Presentation only. Logical actions are committed by Explorer before this segment is published.
class Motion {
    WorldPoint from_{}, to_{};
    int64_t start_ = 0, duration_ = kMoveDurationUs, pausedAt_ = 0;
    bool paused_ = false, started_ = false;
public:
    uint32_t actorId = 0; uint64_t actionId = 0; unsigned direction = 0;
    void reset(WorldPoint p) { from_=to_=p; started_=paused_=false; }
    void begin(uint32_t actor, uint64_t action, WorldPoint from, WorldPoint to, int64_t now, unsigned dir, int64_t duration=kMoveDurationUs) {
        actorId=actor; actionId=action; from_=from; to_=to; start_=now; duration_=std::max<int64_t>(1,duration); direction=dir; started_=true; paused_=false;
    }
    WorldPoint sample(int64_t now) const {
        const double t=started_? std::clamp(double((paused_?pausedAt_:now)-start_)/duration_,0.0,1.0):1.0;
        return {from_.x+(to_.x-from_.x)*t,from_.y+(to_.y-from_.y)*t};
    }
    bool active(int64_t now) const { return started_ && (paused_?pausedAt_:now)<start_+duration_; }
    std::optional<int64_t> nextDeadline(int64_t now) const {
        if(paused_ || !active(now)) return {};
        return std::min(start_+duration_,now+16667);
    }
    void pause(int64_t now) { if(!paused_) { paused_=true;pausedAt_=now; } }
    void resume(int64_t now) { if(paused_) { start_+=now-pausedAt_;paused_=false; } }
};
}
