#include "core/exploration_package.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <algorithm>

int main(int argc, char** argv) {
    try {
        if (argc < 3 || argc > 5) throw std::runtime_error("usage: pet_explore package.mdpe seed [snapshot.mdes] [--grid|--gameplay]");
        const auto package = pet::loadExplorationPackage(argv[1]);
        std::size_t used = 0; const auto seed = std::stoull(argv[2], &used);
        if (used != std::string(argv[2]).size() || argv[2][0] == '-') throw std::runtime_error("invalid seed");
        const bool grid = std::string(argv[argc - 1]) == "--grid";
        if (grid) {
            pet::Rng rng{seed}; const auto world = pet::generate(rng);
            const auto chunks = pet::chunkGrid(world, package.tileset(package.floors.front().tilesetId));
            std::cout << "{\"terrain\":[";
            for (int i = 0; i < pet::kTiles; ++i) std::cout << (i ? "," : "") << int(world.terrain[i]);
            std::cout << "],\"chunks\":[";
            for (int i = 0; i < pet::kTiles; ++i) std::cout << (i ? "," : "") << chunks[i];
            std::cout << "]}\n"; return 0;
        }
        auto explorer = pet::Explorer(seed + 1, seed, package.assetVersion, package.floors);
        const std::filesystem::path statePath = argc >= 4 ? argv[3] : "";
        if (!statePath.empty() && std::filesystem::exists(statePath))
            explorer = pet::Explorer(pet::loadSnapshot(statePath, package.assetVersion));
        unsigned ticks = 0, pickups = 0, greetings = 0, clears = 0;
        auto time = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        if (std::string(argv[argc - 1]) == "--gameplay") explorer.enableGameplay(time);
        if (explorer.state().gameplay.enabled) time = std::max(time, explorer.state().gameplay.phaseStartedMs);
        // Headless acceptance stops at Ready/RoomExit, before the next-day transaction.
        while (explorer.state().gameplay.enabled ? explorer.state().gameplay.life != pet::LifePhase::Ready : explorer.state().phase != pet::Phase::Home) {
            auto candidate = explorer;
            pet::Event event;
            if (candidate.state().gameplay.enabled) {
                time += 350; // Headless simulation; Windows uses real UTC and clip completions.
                const auto update = candidate.advanceGameplay(time, true);
                if (update.event == pet::GameplayEvent::Pickup) event.kind = pet::EventKind::Pickup;
                if (update.event == pet::GameplayEvent::Greeting) event.kind = pet::EventKind::Greeting;
                if (update.event == pet::GameplayEvent::Pose) event.kind = pet::EventKind::Pose;
            } else event = candidate.advance();
            if (!statePath.empty()) pet::saveSnapshot(statePath, candidate.state());
            explorer = std::move(candidate); // publish only after durable commit
            ++ticks;
            pickups += event.kind == pet::EventKind::Pickup;
            greetings += event.kind == pet::EventKind::Greeting;
            clears += event.kind == pet::EventKind::Pose;
            if (ticks > 128 * (pet::Explorer::kFloorStepLimit + pet::kTiles + 10))
                throw std::runtime_error("exploration exceeded finite bound");
        }
        std::cout << "{\"completed\":" << (explorer.state().completed ? "true" : "false")
                  << ",\"ticks\":" << ticks << ",\"newPickups\":" << pickups << ",\"newGreetings\":" << greetings
                  << ",\"newClears\":" << clears << ",\"inventory\":" << explorer.state().inventory.size()
                  << ",\"rngState\":" << explorer.state().rng.state
                  << ",\"day\":" << explorer.state().gameplay.day << ",\"hearts\":" << explorer.state().gameplay.hearts
                  << ",\"life\":\"" << pet::lifeName(explorer.state().gameplay.life) << "\""
                  << ",\"sceneBeat\":" << static_cast<unsigned>(explorer.state().gameplay.sceneBeat)
                  << ",\"oran\":" << explorer.state().gameplay.oran << ",\"gameplayTransactions\":" << explorer.state().gameplay.onceKeys.size() << "}\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
