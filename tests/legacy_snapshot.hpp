#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>
// MDES8 stores its party payload length before CRC. Remove it before applying
// the fixed MDES7 companion block and the older byte-shape fixture conversions.
inline void removePartyBlock(std::vector<std::uint8_t>& bytes) {
    if(bytes.size()<12 || bytes[4]<8)return;
    std::uint32_t length=0;
    for(unsigned b=0;b<4;++b)length|=std::uint32_t(bytes[bytes.size()-8+b])<<(8*b);
    if(length>bytes.size()-16)throw std::runtime_error("invalid MDES8 test payload");
    bytes.erase(bytes.end()-8-length,bytes.end()-4);bytes[4]=7;
}
inline void removeCompanionBlock(std::vector<std::uint8_t>& bytes) {
    removePartyBlock(bytes);bytes.erase(bytes.end()-80,bytes.end()-4);
}
