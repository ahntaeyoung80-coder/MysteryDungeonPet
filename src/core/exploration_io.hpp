#pragma once
#include "core/exploration.hpp"
#include <stdexcept>

namespace pet::detail {
constexpr std::size_t kFileLimit = 8 * 1024 * 1024;
struct Writer {
    std::vector<std::uint8_t> bytes;
    void u32(std::uint32_t n) { for (int i = 0; i < 4; ++i) bytes.push_back(std::uint8_t(n >> (i * 8))); }
    void u64(std::uint64_t n) { u32(std::uint32_t(n)); u32(std::uint32_t(n >> 32)); }
    void str(const std::string& s) { u32(static_cast<std::uint32_t>(s.size())); bytes.insert(bytes.end(), s.begin(), s.end()); }
};
struct Reader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t offset = 0;
    explicit Reader(const std::vector<std::uint8_t>& b) : bytes(b) {
        if (b.size() > kFileLimit) throw std::runtime_error("exploration file exceeds byte limit");
    }
    std::uint32_t u32() {
        if (bytes.size() - offset < 4) throw std::runtime_error("truncated exploration field");
        std::uint32_t n = 0;
        for (int i = 0; i < 4; ++i) n |= std::uint32_t(bytes[offset++]) << (i * 8);
        return n;
    }
    std::uint64_t u64() { const auto low = u32(); const auto high = u32(); return low | (std::uint64_t(high) << 32); }
    std::uint32_t count(std::uint32_t max) {
        const auto n = u32();
        if (n > max) throw std::runtime_error("exploration count exceeds limit");
        return n;
    }
    bool boolean() { return count(1) != 0; }
    std::string str() {
        const auto n = count(256);
        if (n > bytes.size() - offset) throw std::runtime_error("truncated exploration string");
        std::string s(bytes.begin() + offset, bytes.begin() + offset + n);
        offset += n;
        if (s.find('\0') != std::string::npos) throw std::runtime_error("NUL in exploration string");
        return s;
    }
    int tile() { return static_cast<int>(count(kTiles - 1)); }
    void end() { if (offset != bytes.size()) throw std::runtime_error("trailing exploration bytes"); }
};
void writeProfiles(Writer&, const std::vector<FloorProfile>&);
std::vector<FloorProfile> readProfiles(Reader&);
} // namespace pet::detail
