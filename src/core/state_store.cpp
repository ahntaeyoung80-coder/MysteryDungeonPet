#include "core/exploration_io.hpp"
#include "core/package.hpp"
#include <cstdio>
#include <limits>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace pet {
namespace detail {
void writeWeights(Writer& w, const Weights& weights) {
    w.u32(static_cast<std::uint32_t>(weights.size()));
    for (const auto& entry : weights) { w.u32(entry.originalId); w.u64(entry.value); }
}
Weights readWeights(Reader& r) {
    Weights weights;
    auto n = r.count(65536);
    for (std::uint32_t i = 0; i < n; ++i) {
        const auto id = r.u32(); const auto weight = r.u64();
        weights.push_back({id, weight});
    }
    return weights;
}
void writeProfiles(Writer& w, const std::vector<FloorProfile>& profiles) {
    w.u32(static_cast<std::uint32_t>(profiles.size()));
    for (const auto& p : profiles) {
        w.str(p.edition); w.u32(p.dungeonId); w.u32(p.localFloor);
        w.u32(p.tilesetId); w.u32(p.groupFloor);
        w.u32(static_cast<std::uint32_t>(p.floorItems.size()));
        for (const auto& c : p.floorItems) { w.u32(c.originalId); w.u64(c.weight); writeWeights(w, c.items); }
        writeWeights(w, p.normalFriends);
        w.u32(static_cast<std::uint32_t>(p.guaranteed.size()));
        for (auto id : p.guaranteed) w.u32(id);
    }
}
std::vector<FloorProfile> readProfiles(Reader& r) {
    std::vector<FloorProfile> profiles;
    auto n = r.count(128);
    for (std::uint32_t i = 0; i < n; ++i) {
        FloorProfile p;
        p.edition = r.str(); p.dungeonId = r.u32(); p.localFloor = r.u32();
        p.tilesetId = r.u32(); p.groupFloor = r.u32();
        const auto categories = r.count(1024);
        for (std::uint32_t j = 0; j < categories; ++j) {
            ItemCategory c; c.originalId = r.u32(); c.weight = r.u64(); c.items = readWeights(r);
            p.floorItems.push_back(std::move(c));
        }
        p.normalFriends = readWeights(r);
        const auto guaranteed = r.count(65536);
        for (std::uint32_t j = 0; j < guaranteed; ++j) p.guaranteed.push_back(r.u32());
        profiles.push_back(std::move(p));
    }
    return profiles;
}
}
namespace {
std::uint32_t checksum(const std::vector<std::uint8_t>& bytes, std::size_t end) {
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t i = 0; i < end; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
    }
    return ~crc;
}
}
std::vector<std::uint8_t> encodeSnapshot(const ExplorationState& s) {
    validateState(s);
    detail::Writer w;
    w.u32(0x5345444d); w.u32(1); // MDES, schema 1
    w.str(s.assetVersion); w.u64(s.runId); w.u64(s.mapSeed); w.u64(s.rng.state);
    detail::writeProfiles(w, s.floors);
    w.u32(s.spawnPolicy.itemRequest); w.u32(s.spawnPolicy.friendRequest);
    w.u32(s.spawnPolicy.itemBudget); w.u32(s.spawnPolicy.friendBudget);
    w.u32(s.floorIndex); w.u64(s.world.version);
    for (auto tile : s.world.terrain) w.u32(tile);
    for (auto room : s.world.room) w.u32(static_cast<std::uint32_t>(room + 1));
    w.u32(static_cast<std::uint32_t>(s.world.centers.size()));
    for (auto tile : s.world.centers) w.u32(tile);
    w.u32(s.world.start); w.u32(s.world.stairs);
    w.u32(static_cast<std::uint32_t>(s.population.entities.size()));
    for (const auto& e : s.population.entities) {
        w.u32(e.instanceId); w.u32(e.originalId); w.u32(static_cast<unsigned>(e.kind));
        w.u32(e.tile); w.u32(e.done);
    }
    w.u32(static_cast<std::uint32_t>(s.population.omissions.size()));
    for (const auto& o : s.population.omissions) {
        w.u32(static_cast<unsigned>(o.kind)); w.u32(static_cast<unsigned>(o.reason)); w.u32(o.count); w.u32(o.guaranteed);
    }
    w.u32(s.position);
    for (auto tile : s.visited) w.u32(tile);
    for (auto tile : s.blocked) w.u32(tile);
    w.u64(s.occupancyVersion); w.u32(s.floorSteps); w.u32(s.poseSteps);
    w.u32(static_cast<unsigned>(s.phase)); w.u32(s.completed);
    w.u32(static_cast<std::uint32_t>(s.inventory.size()));
    for (const auto& item : s.inventory) { w.str(item.onceKey); w.u32(item.originalId); }
    w.u32(static_cast<std::uint32_t>(s.onceKeys.size()));
    for (const auto& key : s.onceKeys) w.str(key);
    w.u32(static_cast<std::uint32_t>(s.target + 1)); w.u32(static_cast<unsigned>(s.goalKind));
    w.u32(checksum(w.bytes, w.bytes.size()));
    if (w.bytes.size() > detail::kFileLimit) throw std::runtime_error("snapshot exceeds byte limit");
    return w.bytes;
}
ExplorationState decodeSnapshot(const std::vector<std::uint8_t>& bytes, const std::string& expected) {
    detail::Reader r(bytes);
    if (r.u32() != 0x5345444d || r.u32() != 1) throw std::runtime_error("unknown snapshot schema");
    if (bytes.size() < 12) throw std::runtime_error("truncated snapshot");
    detail::Reader tail(bytes); tail.offset = bytes.size() - 4;
    if (tail.u32() != checksum(bytes, bytes.size() - 4)) throw std::runtime_error("snapshot checksum mismatch");
    ExplorationState s;
    s.assetVersion = r.str();
    if (s.assetVersion != expected) throw std::runtime_error("snapshot asset version mismatch");
    s.runId = r.u64(); s.mapSeed = r.u64(); s.rng.state = r.u64();
    s.floors = detail::readProfiles(r);
    s.spawnPolicy.itemRequest = r.u32(); s.spawnPolicy.friendRequest = r.u32();
    s.spawnPolicy.itemBudget = r.u32(); s.spawnPolicy.friendBudget = r.u32();
    s.floorIndex = r.count(127); s.world.version = r.u64();
    for (auto& tile : s.world.terrain) tile = static_cast<std::uint8_t>(r.count(1));
    for (auto& room : s.world.room) room = static_cast<std::int8_t>(static_cast<int>(r.count(3)) - 1);
    auto n = r.count(3);
    for (std::uint32_t i = 0; i < n; ++i) s.world.centers.push_back(r.tile());
    s.world.start = r.tile(); s.world.stairs = r.tile();
    n = r.count(kTiles);
    for (std::uint32_t i = 0; i < n; ++i) {
        Entity e;
        e.instanceId = r.u32(); e.originalId = r.u32(); e.kind = static_cast<EntityKind>(r.count(1));
        e.tile = r.tile(); e.done = r.boolean(); s.population.entities.push_back(e);
    }
    n = r.count(16);
    for (std::uint32_t i = 0; i < n; ++i) {
        Omission o;
        o.kind = static_cast<EntityKind>(r.count(1)); o.reason = static_cast<OmissionReason>(r.count(2));
        o.count = r.u32(); o.guaranteed = r.boolean(); s.population.omissions.push_back(o);
    }
    s.position = r.tile();
    for (auto& tile : s.visited) tile = static_cast<std::uint8_t>(r.count(1));
    for (auto& tile : s.blocked) tile = static_cast<std::uint8_t>(r.count(1));
    s.occupancyVersion = r.u64(); s.floorSteps = r.u32(); s.poseSteps = r.u32();
    s.phase = static_cast<Phase>(r.count(2)); s.completed = r.boolean();
    n = r.count(128 * kTiles);
    for (std::uint32_t i = 0; i < n; ++i) {
        auto key = r.str(); auto id = r.u32(); s.inventory.push_back({key, id});
    }
    n = r.count(128 * (kTiles + 2));
    for (std::uint32_t i = 0; i < n; ++i)
        if (!s.onceKeys.insert(r.str()).second) throw std::runtime_error("duplicate onceKey");
    s.target = static_cast<int>(r.count(kTiles)) - 1; s.goalKind = static_cast<GoalKind>(r.count(4));
    r.u32(); r.end();
    validateState(s);
    return s;
}
void saveSnapshot(const std::filesystem::path& path, const ExplorationState& state) {
    const auto bytes = encodeSnapshot(state);
    auto temporary = path; temporary += ".tmp";
#ifdef _WIN32
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("snapshot CreateFile OS=" + std::to_string(GetLastError()));
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
                    written == bytes.size() && FlushFileBuffers(file);
    const auto error = GetLastError(); CloseHandle(file);
    if (!ok) throw std::runtime_error("snapshot write/flush OS=" + std::to_string(error));
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("snapshot replace OS=" + std::to_string(GetLastError()));
#else
    const int file = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file < 0) throw std::system_error(errno, std::generic_category(), "snapshot open");
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto n = ::write(file, bytes.data() + offset, bytes.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { const auto error = errno; ::close(file); throw std::system_error(error, std::generic_category(), "snapshot write"); }
        offset += static_cast<std::size_t>(n);
    }
    if (::fsync(file) != 0) { const auto error = errno; ::close(file); throw std::system_error(error, std::generic_category(), "snapshot flush"); }
    if (::close(file) != 0) throw std::system_error(errno, std::generic_category(), "snapshot close");
    if (::rename(temporary.c_str(), path.c_str()) != 0) throw std::system_error(errno, std::generic_category(), "snapshot replace");
    auto parent = path.parent_path(); if (parent.empty()) parent = ".";
    const int directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (directory < 0) throw std::system_error(errno, std::generic_category(), "snapshot directory open");
    const auto status = ::fsync(directory); const auto error = errno; ::close(directory);
    if (status != 0) throw std::system_error(error, std::generic_category(), "snapshot directory flush");
#endif
}
ExplorationState loadSnapshot(const std::filesystem::path& path, const std::string& expected) {
    return decodeSnapshot(readFile(path, detail::kFileLimit), expected);
}
} // namespace pet
