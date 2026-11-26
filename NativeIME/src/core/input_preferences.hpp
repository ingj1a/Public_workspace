#pragma once
#include <cstdint>
namespace nativeime::core {
struct InputPreferences {
    bool modeShortcut=true;
    bool candidateEdges=false;
};
constexpr std::uint32_t EncodePreferences(InputPreferences p) noexcept {
    return (p.modeShortcut?1u:0u)|(p.candidateEdges?2u:0u);
}
constexpr InputPreferences DecodePreferences(std::uint32_t bits) noexcept {
    return bits<=3?InputPreferences{(bits&1u)!=0,(bits&2u)!=0}:InputPreferences{};
}
}
