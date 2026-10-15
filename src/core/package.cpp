#include "core/package.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace pet {
namespace {
[[noreturn]] void invalid(const char* reason) { throw std::runtime_error(std::string("package: ") + reason); }
class Reader {
    const std::vector<uint8_t>& bytes;
    size_t offset = 0;
public:
    explicit Reader(const std::vector<uint8_t>& b) : bytes(b) {}
    uint32_t u32() {
        if (bytes.size() - offset < 4) invalid("truncated field");
        uint32_t n = 0;
        for (unsigned i = 0; i < 4; ++i) n |= uint32_t(bytes[offset++]) << (i * 8);
        return n;
    }
    int32_t i32() {
        const auto n = u32();
        // Avoid implementation-defined unsigned -> signed overflow.
        return n <= 0x7fffffffU ? int32_t(n) : int32_t(int64_t(n) - 0x100000000LL);
    }
    std::string str(size_t max) {
        auto n = u32();
        if (n > max || n > bytes.size() - offset) invalid("invalid string length");
        std::string s(reinterpret_cast<const char*>(bytes.data() + offset), n);
        offset += n;
        if (s.find('\0') != std::string::npos) invalid("embedded NUL");
        // Strict UTF-8, rejecting overlong sequences, surrogates and > U+10FFFF.
        for (size_t i = 0; i < s.size();) {
            uint32_t c = uint8_t(s[i++]);
            if (c < 0x80) continue;
            unsigned count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 0;
            if (!count || count > s.size() - i) invalid("invalid UTF-8");
            uint32_t value = c & ((1U << (6 - count)) - 1);
            for (unsigned j = 0; j < count; ++j) {
                uint32_t b = uint8_t(s[i++]);
                if ((b & 0xc0) != 0x80) invalid("invalid UTF-8");
                value = (value << 6) | (b & 63);
            }
            if (value < (count == 1 ? 0x80U : count == 2 ? 0x800U : 0x10000U) ||
                value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) invalid("invalid UTF-8");
        }
        return s;
    }
    bool done() const { return offset == bytes.size(); }
};
}
std::vector<uint8_t> readFile(const std::filesystem::path& path, size_t limit) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("cannot open: " + path.u8string());
    auto size = f.tellg();
    if (size < 0 || uint64_t(size) > limit) throw std::runtime_error("file size limit: " + path.u8string());
    std::vector<uint8_t> b(static_cast<size_t>(size));
    f.seekg(0);
    if (!b.empty() && !f.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(b.size())))
        throw std::runtime_error("file read: " + path.u8string());
    return b;
}
Package loadPackage(const std::filesystem::path& p) { return parsePackage(readFile(p, 2 * 1024 * 1024)); }
const Clip& Package::resolve(uint32_t id) const {
    for (size_t depth = 0; depth <= clips.size(); ++depth) {
        auto it = std::find_if(clips.begin(), clips.end(), [id](const Clip& c) { return c.id == id; });
        if (it == clips.end()) invalid("missing clip/alias target");
        if (it->aliasOf < 0) return *it;
        id = uint32_t(it->aliasOf);
    }
    invalid("alias cycle");
}
const std::vector<Frame>& Package::frames(uint32_t id, unsigned direction) const {
    if (direction > 7) invalid("direction outside 0..7");
    auto it = std::find_if(clips.begin(), clips.end(), [id](const Clip& c) { return c.id == id; });
    if (it == clips.end()) invalid("missing clip");
    const auto& c = it->rows.empty() ? resolve(id) : *it;
    if (c.rows.empty()) invalid("empty clip");
    return c.rows[c.rows.size() == 1 ? 0 : direction];
}
Package parsePackage(const std::vector<uint8_t>& bytes) {
    if (bytes.size() > 2 * 1024 * 1024) invalid("manifest size limit");
    Reader r(bytes);
    if (r.u32() != 0x4150444d || r.u32() != 1) invalid("magic/schema version");
    Package p;
    p.assetVersion = r.u32(); p.tickHz = r.u32(); p.width = r.u32(); p.height = r.u32();
    p.pngBytes = r.u32(); p.sourceKind = r.u32(); p.shadowSize = r.u32();
    if (!p.assetVersion || !p.tickHz || p.tickHz > 1000 || !p.width || !p.height ||
        uint64_t(p.width) * p.height > 2 * 1024 * 1024 || !p.pngBytes || p.pngBytes > 8 * 1024 * 1024 || p.sourceKind > 2 || p.shadowSize > 2)
        invalid("header range/cache budget");
    p.atlasName = r.str(128); p.pngSha256 = r.str(64); p.assetKey = r.str(256); p.provenance = r.str(16384);
    if (p.atlasName.empty() || p.atlasName == "." || p.atlasName == ".." || p.atlasName.find_first_of("/\\:") != std::string::npos ||
        p.atlasName.find("..") != std::string::npos) invalid("atlas must be a local basename");
    if (p.pngSha256.size() != 64 || !std::all_of(p.pngSha256.begin(), p.pngSha256.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
        invalid("SHA-256 format");
    if (p.assetKey.empty() || p.provenance.empty()) invalid("missing provenance");
    auto clipCount = r.u32();
    if (!clipCount || clipCount > 256) invalid("clip count");
    std::set<uint32_t> ids;
    size_t totalFrames = 0;
    for (uint32_t i = 0; i < clipCount; ++i) {
        Clip c;
        c.id = r.u32(); c.sourceWanId = r.i32(); c.aliasOf = r.i32(); c.rawFlags = r.u32();
        c.speedNumerator = r.u32(); c.speedDenominator = r.u32(); c.name = r.str(128); c.sourceMetadata = r.str(16384);
        if (c.id > INT32_MAX || !ids.insert(c.id).second || c.name.empty() || c.sourceWanId < -1 || c.aliasOf < -1 ||
            !c.speedNumerator || c.speedNumerator > 1000 || !c.speedDenominator || c.speedDenominator > 1000) invalid("clip metadata");
        auto rows = r.u32();
        if ((rows != 0 && rows != 1 && rows != 8) || (c.aliasOf < 0 && rows == 0)) invalid("direction rows/alias");
        for (uint32_t row = 0; row < rows; ++row) {
            auto count = r.u32();
            totalFrames += count;
            if (!count || count > 4096 || totalFrames > 16384) invalid("frame count");
            std::vector<Frame> frames;
            uint64_t totalTicks = 0;
            for (uint32_t j = 0; j < count; ++j) {
                Frame f;
                f.rect = {r.i32(), r.i32(), r.i32(), r.i32()};
                f.anchor = {r.i32(), r.i32()}; f.shadow = {r.i32(), r.i32()}; f.ticks = r.u32();
                if (f.rect.x < 0 || f.rect.y < 0 || f.rect.w <= 0 || f.rect.h <= 0 ||
                    int64_t(f.rect.x) + f.rect.w > p.width || int64_t(f.rect.y) + f.rect.h > p.height || !f.ticks)
                    invalid("frame rect/ticks");
                totalTicks += f.ticks;
                if (totalTicks > 3600000) invalid("clip duration limit");
                auto bounded = [](Point q) { return q.x >= -1000000 && q.x <= 1000000 && q.y >= -1000000 && q.y <= 1000000; };
                if (!bounded(f.anchor) || !bounded(f.shadow)) invalid("anchor/shadow range");
                auto markers = r.u32();
                if (markers > 16) invalid("marker count");
                std::set<uint32_t> kinds;
                for (uint32_t m = 0; m < markers; ++m) {
                    Marker marker{r.u32(), {r.i32(), r.i32()}};
                    if (!kinds.insert(marker.kind).second || !bounded(marker.point)) invalid("marker metadata");
                    f.markers.push_back(marker);
                }
                f.shadowRect = {r.i32(), r.i32(), r.i32(), r.i32()};
                f.cropOrigin = {r.i32(), r.i32()}; f.rawFlag = r.u32();
                f.rawOffset = {r.i32(), r.i32()}; f.rawShadow = {r.i32(), r.i32()}; f.events = r.u32();
                f.sourceMetadata = r.str(4096);
                if (f.events > 7 || f.cropOrigin.x < 0 || f.cropOrigin.y < 0 || !bounded(f.cropOrigin) ||
                    !bounded(f.rawOffset) || !bounded(f.rawShadow)) invalid("source frame metadata");
                const auto& shadow = f.shadowRect;
                if (shadow.w != 0 || shadow.h != 0) {
                    if (shadow.x < 0 || shadow.y < 0 || shadow.w <= 0 || shadow.h <= 0 ||
                        int64_t(shadow.x) + shadow.w > p.width || int64_t(shadow.y) + shadow.h > p.height)
                        invalid("shadow rect");
                } else if (shadow.x != 0 || shadow.y != 0) invalid("absent shadow rect");
                frames.push_back(std::move(f));
            }
            c.rows.push_back(std::move(frames));
        }
        p.clips.push_back(std::move(c));
    }
    if (!r.done()) invalid("trailing bytes");
    for (const auto& c : p.clips) p.resolve(c.id);
    return p;
}
}
