#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pet {
struct Point { int32_t x = 0, y = 0; };
struct Rect { int32_t x = 0, y = 0, w = 0, h = 0; };
struct Marker { uint32_t kind = 0; Point point; };
struct Frame {
    Rect rect;
    Point anchor, shadow;
    Rect shadowRect;
    Point cropOrigin, rawOffset, rawShadow;
    uint32_t rawFlag = 0xffffffffU, events = 0;
    std::string sourceMetadata;
    uint32_t ticks = 0;
    std::vector<Marker> markers;
};
struct Clip {
    uint32_t id = 0;
    int32_t sourceWanId = -1;
    int32_t aliasOf = -1;
    uint32_t rawFlags = 0;
    uint32_t speedNumerator = 1, speedDenominator = 1;
    std::string name, sourceMetadata;
    std::vector<std::vector<Frame>> rows;
};
struct Package {
    uint32_t assetVersion = 0, tickHz = 0;
    uint32_t width = 0, height = 0, pngBytes = 0, sourceKind = 0;
    uint32_t shadowSize = 0;
    std::string atlasName, pngSha256, assetKey, provenance;
    std::vector<Clip> clips;
    const Clip& resolve(uint32_t id) const;
    const std::vector<Frame>& frames(uint32_t id, unsigned direction) const;
};
Package parsePackage(const std::vector<uint8_t>& bytes);
Package loadPackage(const std::filesystem::path& path);
std::vector<uint8_t> readFile(const std::filesystem::path& path, size_t limit);
}
