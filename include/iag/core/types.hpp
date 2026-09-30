#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace iag {

struct ObjectId {
    std::string value;

    friend bool operator==(const ObjectId&, const ObjectId&) = default;
    friend bool operator<(const ObjectId& lhs, const ObjectId& rhs) {
        return lhs.value < rhs.value;
    }
};

using Revision = std::uint64_t;
using SimMinute = std::int64_t;

enum class Quality {
    Valid,
    Uncertain,
    Stale,
    Invalid,
    Blocked,
};

std::string to_string(Quality quality);
std::string digest(std::string_view canonical);

}  // namespace iag

