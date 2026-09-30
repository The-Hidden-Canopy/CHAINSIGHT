#include "iag/core/types.hpp"

#include <iomanip>
#include <sstream>

namespace iag {

std::string to_string(Quality quality) {
    switch (quality) {
        case Quality::Valid: return "valid";
        case Quality::Uncertain: return "uncertain";
        case Quality::Stale: return "stale";
        case Quality::Invalid: return "invalid";
        case Quality::Blocked: return "blocked";
    }
    return "invalid";
}

std::string digest(std::string_view canonical) {
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    for (const unsigned char ch : canonical) {
        hash ^= ch;
        hash *= prime;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

}  // namespace iag

