#include "core/surface.hpp"
#include <algorithm>
#include <stdexcept>
namespace pet {
Surface::Surface(uint32_t w, uint32_t h) : width(w), height(h) {
    const uint64_t pixelCount = uint64_t(w) * h;
    if (!w || !h || pixelCount > 2 * 1024 * 1024) throw std::runtime_error("surface size/cache budget");
    pixels.resize(size_t(pixelCount * 4));
}
void Surface::clear() { std::fill(pixels.begin(), pixels.end(), 0); }
void Surface::validatePbgra() const {
    if (!width || !height || uint64_t(width) * height > 2 * 1024 * 1024 || pixels.size() != uint64_t(width) * height * 4) throw std::runtime_error("surface stride/size");
    for (size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] > pixels[i+3] || pixels[i+1] > pixels[i+3] || pixels[i+2] > pixels[i+3])
            throw std::runtime_error("surface is not premultiplied BGRA");
}
bool compose(Surface& out, const std::vector<Layer>& layers, Point camera, unsigned scale) {
    if (!scale || scale > 8 || !out.width || !out.height || uint64_t(out.width) * out.height > 2 * 1024 * 1024 || out.pixels.size() != uint64_t(out.width) * out.height * 4) throw std::runtime_error("compose size/scale");
    auto previous = out.pixels;
    out.clear();
    for (const auto& layer : layers) {
        if (!layer.atlas || layer.rect.w <= 0 || layer.rect.h <= 0) throw std::runtime_error("invalid layer");
        const auto& src = *layer.atlas;
        if (!src.width || !src.height || uint64_t(src.width) * src.height > 2 * 1024 * 1024 || src.pixels.size() != uint64_t(src.width) * src.height * 4) throw std::runtime_error("source stride/size");
        const int64_t left = (int64_t(layer.scenePosition.x) - layer.anchor.x - camera.x) * scale;
        const int64_t top = (int64_t(layer.scenePosition.y) - layer.anchor.y - camera.y) * scale;
        const int64_t right = left + int64_t(layer.rect.w) * scale, bottom = top + int64_t(layer.rect.h) * scale;
        // Iterate the visible destination only, even for extreme scene coordinates.
        for (int64_t y = std::max<int64_t>(0, top); y < std::min<int64_t>(out.height, bottom); ++y) {
            const int64_t sy = int64_t(layer.rect.y) + (y - top) / scale;
            if (sy < 0 || sy >= src.height) continue;
            for (int64_t x = std::max<int64_t>(0, left); x < std::min<int64_t>(out.width, right); ++x) {
                const int64_t sx = int64_t(layer.rect.x) + (x - left) / scale;
                if (sx < 0 || sx >= src.width) continue;
                const size_t s = size_t(sy * src.width + sx) * 4, d = size_t(y * out.width + x) * 4;
                const unsigned inv = 255 - src.pixels[s + 3];
                for (size_t ch = 0; ch < 4; ++ch)
                    out.pixels[d + ch] = uint8_t(src.pixels[s + ch] + (out.pixels[d + ch] * inv + 127) / 255);
            }
        }
    }
    return previous != out.pixels;
}
}
