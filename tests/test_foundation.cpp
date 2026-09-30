#include "iag/core/types.hpp"
#include "iag/iag.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    check(iag::version() == "0.1.0", "version is stable");
    check(iag::to_string(iag::Quality::Blocked) == "blocked",
          "quality names are stable");
    check(iag::digest("same") == iag::digest("same"),
          "digest is deterministic");
    check(iag::digest("same") != iag::digest("different"),
          "digest distinguishes canonical input");
    check(iag::ObjectId{"M-12"} < iag::ObjectId{"M-13"},
          "object identifiers sort lexically");
    std::cout << "foundation tests passed\n";
}

