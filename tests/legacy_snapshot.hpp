#pragma once
#include <cstdint>
#include <vector>
// MDES7 appends a fixed 76-byte companion block before CRC. Legacy byte-shape
// fixtures below remove it before applying their existing MDES3/4/5 conversions.
inline void removeCompanionBlock(std::vector<std::uint8_t>& bytes) {
    bytes.erase(bytes.end()-80,bytes.end()-4);
}
