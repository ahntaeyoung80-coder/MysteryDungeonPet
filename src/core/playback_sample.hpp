#pragma once
#include "core/package.hpp"
#include <algorithm>
#include <ostream>
#include <stdexcept>

namespace pet {
// Renderer와 계측은 같은 패키지 프레임/장면 위치를 사용한다.
struct PlaybackSample {
    const Frame* frame = nullptr;
    int64_t animationId = -1, direction = -1, resolvedDirection = -1, frameIndex = -1;
    bool active = false;
    Point scenePosition;
    static constexpr const char* columns = "animation_id,direction,resolved_direction,frame_index,animation_active,scene_x,scene_y,frame_ticks,frame_flag,anchor_x,anchor_y,rect_x,rect_y,rect_w,rect_h";
    void writeCsv(std::ostream& out) const {
        const Frame empty{};
        const auto& f = frame ? *frame : empty;
        out << animationId << ',' << direction << ',' << resolvedDirection << ',' << frameIndex << ',' << active << ','
            << scenePosition.x << ',' << scenePosition.y << ',' << f.ticks << ',' << f.rawFlag << ','
            << f.anchor.x << ',' << f.anchor.y << ',' << f.rect.x << ',' << f.rect.y << ',' << f.rect.w << ',' << f.rect.h;
    }
};
inline PlaybackSample playbackSample(const Package& p, uint32_t clip, unsigned direction, size_t index,
                                     bool active, Point scene, bool visibleBody = true) {
    if (!visibleBody) return {};
    const auto it = std::find_if(p.clips.begin(), p.clips.end(), [clip](const Clip& c) { return c.id == clip; });
    if (it == p.clips.end()) throw std::runtime_error("unknown playback sample clip");
    const auto& rows = it->rows.empty() ? p.resolve(clip).rows : it->rows;
    const auto& frames = p.frames(clip, direction);
    if (index >= frames.size()) throw std::runtime_error("playback sample frame out of bounds");
    return {&frames[index], clip, direction, rows.size() == 1 ? 0 : int64_t(direction), int64_t(index), active, scene};
}
}
