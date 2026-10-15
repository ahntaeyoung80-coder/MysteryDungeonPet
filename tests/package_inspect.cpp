#include "core/package.hpp"
#include <iostream>
#include <iomanip>
#include <stdexcept>
namespace {
void hex(const std::string& text) {
    std::cout << '"';
    for (unsigned char c : text) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(c);
    std::cout << std::dec << '"';
}
void point(pet::Point p) { std::cout << '[' << p.x << ',' << p.y << ']'; }
void rect(pet::Rect r) { std::cout << '[' << r.x << ',' << r.y << ',' << r.w << ',' << r.h << ']'; }
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected runtime manifest path");
        auto p = pet::loadPackage(std::filesystem::u8path(argv[1]));
        std::cout << "{\"assetKeyHex\":"; hex(p.assetKey);
        std::cout << ",\"provenanceHex\":"; hex(p.provenance);
        std::cout << ",\"assetVersion\":" << p.assetVersion << ",\"tickHz\":" << p.tickHz << ",\"width\":" << p.width
                  << ",\"height\":" << p.height << ",\"pngBytes\":" << p.pngBytes << ",\"pngSha256\":\"" << p.pngSha256 << "\",\"atlasNameHex\":";
        hex(p.atlasName);
        std::cout << ",\"sourceKind\":" << p.sourceKind << ",\"shadowSize\":" << p.shadowSize << ",\"clips\":[";
        bool firstClip = true;
        for (const auto& c : p.clips) {
            if (!firstClip) std::cout << ',';
            firstClip = false;
            std::cout << "{\"id\":" << c.id << ",\"aliasOf\":" << c.aliasOf << ",\"sourceWanId\":" << c.sourceWanId << ",\"sourceMetadataHex\":";
            hex(c.sourceMetadata); std::cout << ",\"nameHex\":"; hex(c.name);
            std::cout << ",\"speedNumerator\":" << c.speedNumerator << ",\"speedDenominator\":" << c.speedDenominator << ",\"rawFlags\":" << c.rawFlags;
            std::cout << ",\"rows\":[";
            const auto& actual = c.rows.empty() ? p.resolve(c.id).rows : c.rows;
            for (size_t row = 0; row < actual.size(); ++row) {
                if (row) std::cout << ',';
                std::cout << '[';
                for (size_t i = 0; i < actual[row].size(); ++i) {
                    if (i) std::cout << ',';
                    const auto& f = actual[row][i];
                    std::cout << "{\"rect\":"; rect(f.rect);
                    std::cout << ",\"shadowRect\":"; rect(f.shadowRect);
                    std::cout << ",\"anchor\":"; point(f.anchor);
                    std::cout << ",\"shadow\":"; point(f.shadow);
                    std::cout << ",\"cropOrigin\":"; point(f.cropOrigin);
                    std::cout << ",\"rawOffset\":"; point(f.rawOffset);
                    std::cout << ",\"rawShadow\":"; point(f.rawShadow);
                    std::cout << ",\"ticks\":" << f.ticks << ",\"rawFlag\":" << f.rawFlag << ",\"events\":" << f.events << ",\"sourceMetadataHex\":";
                    hex(f.sourceMetadata); std::cout << ",\"markers\":[";
                    for (size_t m = 0; m < f.markers.size(); ++m) {
                        if (m) std::cout << ',';
                        std::cout << "{\"kind\":" << f.markers[m].kind << ",\"point\":"; point(f.markers[m].point); std::cout << '}';
                    }
                    std::cout << "]}";
                }
                std::cout << ']';
            }
            std::cout << "]}";
        }
        std::cout << "]}\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
