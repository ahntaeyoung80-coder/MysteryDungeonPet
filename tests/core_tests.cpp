#include "core/package.hpp"
#include "core/animator.hpp"
#include "core/surface.hpp"
#include "core/playback_sample.hpp"
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <sstream>
using namespace pet;
namespace {
unsigned checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& f, const char* message) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    check(rejected, message);
}
uint32_t u32(const std::vector<uint8_t>& b, size_t offset) {
    return b.at(offset) | uint32_t(b.at(offset+1)) << 8 | uint32_t(b.at(offset+2)) << 16 | uint32_t(b.at(offset+3)) << 24;
}
void put(std::vector<uint8_t>& b, size_t offset, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) b.at(offset + i) = uint8_t(n >> (i*8));
}
void packageTests(const std::vector<uint8_t>& bytes) {
    const auto p = parsePackage(bytes);
    check(p.assetVersion == 1 && p.sourceKind == 0 && p.assetKey == "test:synthetic:a2-v1", "fixture provenance");
    check(p.width == 48 && p.height == 160 && p.pngBytes == 917 && p.clips.size() == 7, "producer/consumer fields");
    check(p.frames(0, 7)[2].rect.y == 140 && p.frames(0, 7)[1].ticks == 10, "8 direction/tick preservation");
    check(p.frames(15, 7).data() == p.frames(15, 0).data(), "single row must remain a single row");
    check(p.frames(107, 4).data() == p.frames(7, 4).data(), "alias resolves without duplicate PNG");
    check(p.frames(0, 0)[0].anchor.y == 16 && p.frames(0, 0)[0].markers[1].point.y == 4, "anchor/marker preservation");
    rejects([&] { p.frames(0, 8); }, "direction 8");
    rejects([&] { p.frames(999, 0); }, "unknown selector");
    for (size_t n = 0; n < bytes.size(); ++n) {
        std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + n);
        rejects([&] { parsePackage(truncated); }, "truncated manifest accepted");
    }
    auto b = bytes; b.push_back(0);
    rejects([&] { parsePackage(b); }, "trailing bytes");
    for (const auto& [offset, value] : std::vector<std::pair<size_t, uint32_t>>{{0, 0}, {4, 2}, {8, 0}, {12, 0}, {16, 0xffffffff}, {20, 0}, {24, 0xffffffff}, {28, 3}, {36, 0xffffffff}}) {
        b = bytes; put(b, offset, value);
        rejects([&] { parsePackage(b); }, "header overflow/version/budget");
    }
    b = bytes; b.at(40) = '/';
    rejects([&] { parsePackage(b); }, "atlas traversal");
    b = bytes; b.at(40) = 0;
    rejects([&] { parsePackage(b); }, "NUL basename");
    b = bytes; b.at(40) = 0xc0;
    rejects([&] { parsePackage(b); }, "overlong UTF-8");
    size_t cursor = 36;
    for (unsigned i = 0; i < 4; ++i) cursor += 4 + u32(bytes, cursor);
    size_t clipCount = cursor; cursor += 4;
    const size_t firstClip = cursor;
    cursor += 24; cursor += 4 + u32(bytes, cursor); cursor += 4 + u32(bytes, cursor); // name + source metadata
    const size_t firstRows = cursor; cursor += 4;
    const size_t firstCount = cursor; cursor += 4;
    const size_t firstFrame = cursor;
    for (const auto& [offset, value] : std::vector<std::pair<size_t, uint32_t>>{
        {clipCount, 0xffffffff}, {firstRows, 7}, {firstCount, 0xffffffff},
        {firstFrame, 0xffffffff}, {firstFrame+8, 0}, {firstFrame+12, 0xffffffff},
        {firstFrame+16, 0x7fffffff}, {firstFrame+32, 0}, {firstFrame+36, 17}}) {
        b = bytes; put(b, offset, value);
        rejects([&] { parsePackage(b); }, "frame/count/anchor bound");
    }
    b = bytes; put(b, firstClip+8, 0);
    rejects([&] { parsePackage(b); }, "self alias cycle");
    auto cycle = p;
    cycle.clips[0].aliasOf = 107; cycle.clips[1].aliasOf = 0;
    rejects([&] { cycle.resolve(107); }, "alias cycle");
    cycle = p; cycle.clips.back().aliasOf = 999;
    rejects([&] { cycle.resolve(107); }, "broken alias reference");
}
void animationTests(const Package& p) {
    Animator a;
    a.start(p, 0, 0, false, 0);
    auto r = a.advance(0);
    check(r.frame == 0 && r.markers.size() == 1 && r.nextDeadlineUs == 133334, "first frame/deadline");
    check(a.advance(0).markers.empty(), "duplicate first marker");
    check(a.advance(133333).frame == 0, "fractional tick early boundary");
    r = a.advance(133334);
    check(r.frame == 1 && r.markers.size() == 1 && r.nextDeadlineUs == 300000, "exact frame boundary");
    r = a.advance(10000000);
    check(r.finished && r.finishedEvent && r.frame == 2 && r.markers.size() == 1 && !r.nextDeadlineUs, "delayed finite finish");
    check(!a.advance(11000000).finishedEvent && a.advance(11000000).markers.empty(), "finish/markers exactly once");
    a.start(p, 0, 0, true, 0); a.advance(0);
    r = a.advance(1000000000000LL);
    check(!r.finished && r.skippedCycles > 1000000 && r.markers.size() <= 3 && r.nextDeadlineUs > 1000000000000LL, "bounded long stall");
    check(a.advance(1000000000000LL).markers.empty(), "stalled markers repeat");
    a.start(p, 0, 0, false, 0); a.advance(0); a.pause(100000);
    r = a.advance(5000000);
    check(r.frame == 0 && !r.nextDeadlineUs, "pause has no deadline");
    a.resume(5000000);
    check(a.advance(5033333).frame == 0 && a.advance(5033334).frame == 1, "resume excludes pause duration");
    a.start(p, 0, 0, false, 0); a.advance(0); a.pause(500000);
    a.resume(10000000);
    check(a.advance(10000000).finishedEvent, "pause must not swallow finite completion");
    a.stop(); check(!a.active() && !a.advance(11000000).nextDeadlineUs, "explicit sleep stop");
    a.start(p, 5, 7, true, 0);
    check(!a.advance(10000000).finished, "sleep loop never self-finishes");
    rejects([&] { a.advance(1); }, "backward clock");
    rejects([&] { a.start(p, 0, 0, true, -1); }, "negative clock");
    auto fast = p; fast.clips[0].speedNumerator = 2;
    a.start(fast, 0, 0, false, 0);
    check(a.advance(0).nextDeadlineUs == 66667 && a.advance(66667).frame == 1, "speed/ticks independent");
    // Distinct per-frame marker IDs verify chronological ordering across wrap.
    auto order = p;
    for (size_t i = 0; i < 3; ++i) order.clips[0].rows[0][i].events = 1U << i;
    a.start(order, 0, 0, true, 0); a.advance(0);
    r = a.advance(500000);
    check(r.markers.size() == 3 && r.markers[0].kind == 1 && r.markers[1].kind == 2 && r.markers[2].kind == 0, "marker ordering through wrap");
}
void surfaceTests() {
    rejects([] { Surface huge(0xffffffff, 0xffffffff); }, "surface integer overflow");
    rejects([] { Surface wrap(0x80000000U, 0x80000000U); }, "surface multiply wrap");
    rejects([] { Surface zero(0, 1); }, "zero surface");
    Surface source(2, 2), out(4, 4);
    source.pixels = {0,0,0,255, 10,20,30,128, 20,30,40,255, 0,0,0,0};
    source.validatePbgra();
    std::vector<Layer> layers{{&source, {0,0,2,2}, {0,0}, {0,0}}};
    check(compose(out, layers, {0,0}, 2), "initial compose changes");
    check(out.pixels[3] == 255 && out.pixels[7] == 255 && out.pixels[11] == 128 && out.pixels[8] == 10, "nearest/black/alpha preservation");
    check(!compose(out, layers, {0,0}, 2), "identical surface must not submit");
    check(compose(out, layers, {1,1}, 2) && out.pixels[3] == 0, "camera clipping");
    layers[0].scenePosition = {INT32_MAX, INT32_MIN};
    check(!compose(out, layers, {INT32_MIN,INT32_MAX}, 8), "extreme scene coordinates");
    layers[0] = {&source, {-1,-1,3,3}, {0,0}, {-1,-1}};
    check(compose(out, layers, {0,0}, 1) && out.pixels[3] == 255, "source and destination clip");
    Surface below(1,1), above(1,1), blend(1,1);
    below.pixels = {0,0,255,255}; above.pixels = {128,0,0,128};
    compose(blend, {{&below,{0,0,1,1},{0,0},{0,0}}, {&above,{0,0,1,1},{0,0},{0,0}}}, {0,0}, 1);
    check(blend.pixels == std::vector<uint8_t>({128,0,127,255}), "premultiplied source-over exactly once");
    above.pixels[0] = 255; rejects([&] { above.validatePbgra(); }, "straight alpha input rejected");
    rejects([&] { compose(out, layers, {0,0}, 0); }, "zero scale");
    source.pixels.pop_back(); rejects([&] { compose(out, layers, {0,0}, 1); }, "invalid source stride");
}
void playbackSampleTests(const Package& p) {
    Animator animator;
    animator.start(p, 0, 5, true, 0);
    const auto result = animator.advance(133334);
    const auto sample = playbackSample(p, 0, 5, result.frame, animator.active(), {33, 33});
    check(sample.frame == &p.frames(0, 5)[result.frame] && sample.frameIndex == 1, "record uses Animator/Renderer frame");
    check(sample.direction == 5 && sample.resolvedDirection == 5 && sample.animationId == 0, "record selectors");
    std::ostringstream csv; sample.writeCsv(csv);
    check(csv.str() == "0,5,5,1,1,33,33,10,4294967295,8,16,16,100,16,20", "record fields match package frame");
    auto oneRow = playbackSample(p, 15, 7, 0, true, {33,33});
    check(oneRow.direction == 7 && oneRow.resolvedDirection == 0 && oneRow.frame == &p.frames(15, 0)[0], "one row recorded without fake direction");
    auto alias = playbackSample(p, 107, 4, 0, true, {33,33});
    check(alias.animationId == 107 && alias.frame == &p.frames(7,4)[0], "alias selector and resolved frame");
    auto ownAlias = p;
    ownAlias.clips.back().rows = ownAlias.clips[1].rows;
    ownAlias.clips.back().rows[0][0].ticks = 41;
    check(playbackSample(ownAlias, 107, 0, 0, true, {33,33}).frame->ticks == 41, "alias own timing kept in record");
    animator.stop();
    check(!playbackSample(p, 0, 5, result.frame, animator.active(), {33,33}).active, "explicit stop visible in record");
    auto empty = playbackSample(p, 999, 8, 999, false, {33,33}, false);
    check(!empty.frame && !empty.active && empty.animationId == -1 && empty.frameIndex == -1 && empty.scenePosition.x == 0, "empty baseline has no playback");
    rejects([&] { playbackSample(p, 0, 0, 99, true, {33,33}); }, "out of bounds playback record");
}
void mutationSafety(const std::vector<uint8_t>& fixture) {
    std::mt19937 rng(0xA2);
    for (unsigned iteration = 0; iteration < 10000; ++iteration) {
        auto b = fixture;
        for (unsigned j = 0, count = 1 + rng()%8; j < count; ++j) b[rng()%b.size()] = uint8_t(rng());
        try {
            const auto p = parsePackage(b);
            Animator a;
            a.start(p, p.clips.front().id, rng()%8, rng()%2 != 0, 0);
            a.advance(0); a.advance(1000000000);
        } catch (const std::exception&) {}
    }
    ++checks;
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected fixture path");
        auto bytes = readFile(std::filesystem::u8path(argv[1]), 2*1024*1024);
        packageTests(bytes); animationTests(parsePackage(bytes)); playbackSampleTests(parsePackage(bytes)); surfaceTests(); mutationSafety(bytes);
        std::cout << "PASS " << checks << " checks; all truncated prefixes, 10000 seeded mutations\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
