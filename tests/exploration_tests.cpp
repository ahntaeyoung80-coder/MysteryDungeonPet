#include "core/exploration_package.hpp"
#include "core/package.hpp"
#include "core/animator.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace {
unsigned checks = 0;
void check(bool ok, const char* reason) { ++checks; if (!ok) throw std::runtime_error(reason); }
template<class F> void reject(F f, const char* reason) {
    bool failed = false; try { f(); } catch (const std::exception&) { failed = true; }
    check(failed, reason);
}
unsigned finish(pet::Explorer& explorer) {
    unsigned steps = 0;
    while (explorer.state().phase != pet::Phase::Home) {
        explorer.advance();
        check(++steps <= explorer.state().floors.size() * (pet::Explorer::kFloorStepLimit + pet::kTiles + 10),
              "exploration did not terminate");
    }
    pet::validateState(explorer.state()); return steps;
}
struct DistributionResult {
    std::string name;
    pet::Weights weights;
    std::map<std::uint32_t, unsigned> counts;
    double maxSigma = 0;
};
DistributionResult statistics(std::string name, const pet::Weights& weights, pet::Rng& rng) {
    DistributionResult result{std::move(name), weights, {}, 0};
    std::map<std::uint32_t, std::uint64_t> expected;
    std::uint64_t sum = 0;
    for (const auto& w : weights) { expected[w.originalId] += w.value; sum += w.value; }
    constexpr unsigned trials = 100000;
    if (!sum) return result;
    for (unsigned i = 0; i < trials; ++i) { std::uint32_t id; check(pet::sample(weights, rng, id), "weighted sample empty"); ++result.counts[id]; }
    for (const auto& [id, weight] : expected) {
        const double p = double(weight) / double(sum);
        const double error = std::abs(double(result.counts[id]) - trials * p);
        const double deviation = std::sqrt(trials * p * (1 - p));
        const double sigma = deviation ? error / deviation : error;
        result.maxSigma = std::max(result.maxSigma, sigma);
        check(weight || !result.counts[id], "zero weight selected");
        check(sigma <= 7.0, "weighted sampling outside 7 sigma");
    }
    return result;
}
}
int main(int argc, char** argv) {
    try {
        using namespace pet;
        std::array<unsigned, 4> roomCounts{};
        int maxFloor = 0, maxDistance = 0;
        for (std::uint64_t seed = 1; seed <= 10000; ++seed) {
            Rng rng{seed}, repeat{seed}; const auto w = generate(rng), other = generate(repeat);
            validateWorld(w);
            check(w.terrain == other.terrain && w.room == other.room && w.start == other.start &&
                  w.stairs == other.stairs && rng.state == repeat.state, "seed reproduction mismatch");
            const auto search = bfs(w, w.start);
            const int floors = static_cast<int>(std::count(w.terrain.begin(), w.terrain.end(), 1));
            check(std::count_if(search.distance.begin(), search.distance.end(), [](auto d) { return d >= 0; }) == floors,
                  "native geometry disconnected");
            const auto path = search.pathTo(w.stairs);
            check(static_cast<int>(path.size()) == search.distance[w.stairs] && !path.empty(), "BFS not shortest");
            int previous = w.start;
            for (auto tile : path) {
                check(canMove(w,previous,tile),
                      "BFS path jumps/wraps"); previous = tile;
            }
            ++roomCounts[w.centers.size()]; maxFloor = std::max(maxFloor, floors);
            maxDistance = std::max(maxDistance, search.distance[w.stairs]);
        }
        Rng rng{44}; const auto world = generate(rng); const auto profile = fixtureProfiles(1).front();
        Mask full{}; full.fill(1);
        const SpawnPolicy huge{UINT32_MAX, UINT32_MAX, kTiles, kTiles};
        const auto none = populate(world, profile, rng, huge, full);
        check(none.entities.empty() && !none.omissions.empty(), "fully occupied spawn");
        auto crowded = profile; crowded.guaranteed.assign(2000, 77);
        auto population = populate(world, crowded, rng, huge);
        std::set<int> tiles;
        for (const auto& e : population.entities) check(tiles.insert(e.tile).second && e.tile != world.start && e.tile != world.stairs,
                                                       "spawn overlap/start/stairs");
        std::uint64_t omitted = 0;
        for (const auto& o : population.omissions) if (o.guaranteed) omitted += o.count;
        check(omitted + population.entities.size() == 2000, "guaranteed omission accounting");
        auto empty = profile; empty.floorItems.clear(); empty.normalFriends.clear(); empty.guaranteed.clear();
        check(populate(world, empty, rng, {}).entities.empty(), "empty distributions spawn");
        reject([&] { std::uint32_t id; sample({{1, UINT64_MAX}, {2, 1}}, rng, id); }, "weight overflow accepted");
        std::uint32_t id = 99; const auto before = rng.state;
        check(!sample({{1, 0}}, rng, id) && rng.state == before, "zero distribution consumed RNG");
        Mask oneSpace = full;
        for (int p = 0; p < kTiles; ++p) if (world.walkable(p) && p != world.start && p != world.stairs) { oneSpace[p] = 0; break; }
        check(populate(world, profile, rng, huge, oneSpace).entities.size() == 1, "single space overrun");
        std::uint64_t maxSteps = 0;
        for (std::uint64_t seed = 1; seed <= 1000; ++seed) {
            Explorer e(seed, seed, "fixture-v1", fixtureProfiles());
            maxSteps = std::max<std::uint64_t>(maxSteps, finish(e));
            check(e.state().completed && e.state().inventory.size() <= 12, "incomplete fixture dungeon");
            check(e.state().onceKeys.size() >= e.state().inventory.size() + 3 + 1, "duplicate/missing fixture effects");
            const auto bytes = encodeSnapshot(e.state());
            e.advance(); check(bytes == encodeSnapshot(e.state()), "home changes state");
        }
        unsigned restoreSteps = 0, restores = 0;
        for (std::uint64_t seed = 1; seed <= 20; ++seed) {
            Explorer baseline(seed, seed, "fixture-v1", fixtureProfiles());
            Explorer interrupted = baseline;
            while (baseline.state().phase != Phase::Home) {
                const auto saved = encodeSnapshot(interrupted.state());
                interrupted = Explorer(decodeSnapshot(saved, "fixture-v1")); ++restores;
                auto expected = baseline.advance(), actual = interrupted.advance(); ++restoreSteps;
                check(expected.kind == actual.kind && expected.onceKey == actual.onceKey &&
                      encodeSnapshot(baseline.state()) == encodeSnapshot(interrupted.state()), "restore changes continuation/RNG/once");
            }
        }
        Explorer blocked(1, 7, "fixture-v1", fixtureProfiles(1));
        for (const auto& e : blocked.state().population.entities) blocked.setBlocked(e.tile, true);
        finish(blocked);
        check(blocked.state().inventory.empty(), "unreachable item acquired");
        Explorer noExit(2, 8, "fixture-v1", fixtureProfiles(1));
        noExit.setBlocked(noExit.state().world.stairs, true); finish(noExit);
        check(!noExit.state().completed, "blocked stairs cleared dungeon");
        Explorer stale(3, 9, "fixture-v1", fixtureProfiles(1));
        const auto first = stale.advance(); check(first.kind == EventKind::Move, "initial move missing");
        const auto planned = stale.bfsCalls(); stale.advance();
        check(stale.bfsCalls() == planned, "BFS on every move");
        for (int tile = 0; tile < kTiles; ++tile)
            if (stale.state().world.walkable(tile) && tile != stale.state().position && tile != stale.state().world.stairs) {
                stale.setBlocked(tile, true); break;
            }
        finish(stale);
        Explorer dwell(4, 10, "fixture-v1", fixtureProfiles(1));
        auto deadline = dwell.state(); deadline.floorSteps = Explorer::kFloorStepLimit; deadline.knownStairs=deadline.world.stairs; deadline.discovered[deadline.world.stairs]=1;
        dwell = Explorer(deadline); finish(dwell); check(dwell.state().completed, "dwell cap did not route to stairs");
        Explorer saved(5, 11, "fixture-v1", fixtureProfiles(1));
        auto snapshot = encodeSnapshot(saved.state());
        reject([&] { decodeSnapshot(snapshot, "wrong-version"); }, "asset mismatch accepted");
        for (std::size_t i = 0; i < snapshot.size(); ++i) {
            std::vector<std::uint8_t> truncated(snapshot.begin(), snapshot.begin() + i);
            reject([&] { decodeSnapshot(truncated, "fixture-v1"); }, "truncated snapshot accepted");
        }
        Rng corrupt{99};
        for (unsigned i = 0; i < 10000; ++i) {
            auto bytes = snapshot; bytes[corrupt.bounded(bytes.size())] ^= 1U << corrupt.bounded(8);
            reject([&] { decodeSnapshot(bytes, "fixture-v1"); }, "corrupt snapshot accepted");
        }
        auto invalid = saved.state(); invalid.onceKeys.insert("pickup:5:0:500");
        reject([&] { encodeSnapshot(invalid); }, "inconsistent pickup accepted");
        const auto dir = std::filesystem::temp_directory_path() / ("mdpet-b1-" + std::to_string(rng.next()));
        std::filesystem::create_directory(dir);
        const auto path = dir / "state.mdes";
        saveSnapshot(path, saved.state());
        saved.advance(); auto candidate = encodeSnapshot(saved.state());
        { std::ofstream f(path.string() + ".tmp", std::ios::binary); f.write(reinterpret_cast<const char*>(candidate.data()), candidate.size() / 2); }
        check(encodeSnapshot(loadSnapshot(path, "fixture-v1")) == snapshot, "partial temp replaced durable state");
        saveSnapshot(path, saved.state());
        check(encodeSnapshot(loadSnapshot(path, "fixture-v1")) == candidate, "atomic replace lost state");
        reject([&] { auto bad = saved.state(); bad.runId = 0; saveSnapshot(path, bad); }, "invalid save accepted");
        check(encodeSnapshot(loadSnapshot(path, "fixture-v1")) == candidate, "failed save damaged prior state");
        std::filesystem::remove_all(dir);
        std::vector<DistributionResult> distributions;
        distributions.push_back(statistics("synthetic-1-2-7", {{1, 1}, {2, 2}, {3, 7}, {4, 0}}, rng));
        unsigned originalRuns = 0, originalRestores = 0, poseFrames = 0;
        for (int arg = 1; arg < argc; ++arg) {
            const auto p = loadExplorationPackage(argv[arg]);
            for (const auto& f : p.floors) {
                const auto name = std::to_string(f.dungeonId) + ":" + std::to_string(f.localFloor);
                distributions.push_back(statistics(name + ":normal", f.normalFriends, rng));
                Weights categories;
                for (const auto& c : f.floorItems) {
                    categories.push_back({c.originalId, c.weight});
                    distributions.push_back(statistics(name + ":items:" + std::to_string(c.originalId), c.items, rng));
                }
                distributions.push_back(statistics(name + ":categories", categories, rng));
            }
            for (const auto& t : p.tilesets) {
                const auto tileAtlas = loadPackage(std::filesystem::path(argv[arg]).parent_path() / t.packageFile);
                check(tileAtlas.frames(0, 0).size() == t.chunkCount, "tile MDPA count differs");
                for (std::uint64_t seed = 1; seed <= 100; ++seed) {
                    Rng random{seed}; const auto w = generate(random); const auto chunks = chunkGrid(w, t);
                    for (auto chunk : chunks) check(chunk < tileAtlas.frames(0, 0).size(), "autotile outside atlas");
                }
            }
            for (std::uint64_t seed = 1; seed <= 100; ++seed) {
                Explorer original(seed + 5000, seed, p.assetVersion, p.floors);
                while (original.state().phase != Phase::Home) {
                    auto event = original.advance();
                    if (event.kind == EventKind::Pickup || event.kind == EventKind::Greeting ||
                        event.kind == EventKind::FloorChanged || event.kind == EventKind::Pose) {
                        const auto bytes = encodeSnapshot(original.state());
                        original = Explorer(decodeSnapshot(bytes, p.assetVersion)); ++originalRestores;
                    }
                }
                check(original.state().completed, "original dungeon did not clear"); ++originalRuns;
            }
        }
        if (argc > 1) {
            const auto root = std::filesystem::path(argv[1]).parent_path().parent_path().parent_path().parent_path();
            const auto asset = loadPackage(root / "original/runtime/asset-0/asset.mdpa");
            Animator pose; pose.start(asset, 17, 0, false, 0);
            auto result = pose.advance(0);
            unsigned advances = 0;
            while (!result.finished) { check(result.nextDeadlineUs.has_value(), "Pose has no finite deadline"); result = pose.advance(*result.nextDeadlineUs); ++advances; }
            check(result.finishedEvent && !pose.advance(result.nextDeadlineUs.value_or(10000000)).finishedEvent, "Pose completed twice");
            poseFrames = advances;
        }
        std::ostringstream report;
        report << "{\"status\":\"pass\",\"checks\":" << checks
               << ",\"nativeGeometry\":{\"seeds\":[1,10000],\"passed\":10000,\"roomCounts\":{\"2\":"
               << roomCounts[2] << ",\"3\":" << roomCounts[3] << "},\"maxFloorTiles\":" << maxFloor
               << ",\"maxStartStairsDistance\":" << maxDistance << "},\"fixtureRuns\":1000,\"maxFixtureSteps\":" << maxSteps
               << ",\"restoreAtEveryStep\":" << restores << ",\"originalRuns\":" << originalRuns
               << ",\"originalTransactionRestores\":" << originalRestores << ",\"originalPoseAdvances\":" << poseFrames
               << ",\"corruptSnapshotsRejected\":10000,\"truncatedPrefixesRejected\":" << snapshot.size()
               << ",\"distributions\":[";
        for (std::size_t i = 0; i < distributions.size(); ++i) {
            const auto& d = distributions[i]; report << (i ? "," : "") << "{\"name\":\"" << d.name << "\",\"trials\":100000,\"maxSigma\":" << d.maxSigma << ",\"entries\":[";
            for (std::size_t j = 0; j < d.weights.size(); ++j) {
                const auto& w = d.weights[j]; report << (j ? "," : "") << "{\"originalId\":" << w.originalId << ",\"weight\":" << w.value
                                                     << ",\"observed\":" << d.counts.at(w.originalId) << "}";
            }
            report << "]}";
        }
        report << "]}\n";
        std::cout << report.str();
    } catch (const std::exception& e) { std::cerr << "B-1 validation failed: " << e.what() << '\n'; return 1; }
}
