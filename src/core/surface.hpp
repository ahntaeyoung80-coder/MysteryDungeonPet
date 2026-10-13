#pragma once
#include "core/package.hpp"
namespace pet {
struct Surface {
    uint32_t width, height;
    std::vector<uint8_t> pixels; // Premultiplied BGRA, top-down.
    Surface(uint32_t w, uint32_t h);
    void clear();
    void validatePbgra() const;
};
struct Layer { const Surface* atlas; Rect rect; Point anchor, scenePosition; };
bool compose(Surface& output, const std::vector<Layer>& layers, Point camera, unsigned scale);
}
