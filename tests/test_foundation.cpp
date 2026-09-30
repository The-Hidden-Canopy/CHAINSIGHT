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
    check(iag::digest("same") == iag::digest("same"),
          "digest is deterministic");

    auto world = iag::IndustrialWorld::synthetic_enterprise();
    check(world.revision() == 0, "synthetic world starts at revision zero");
    const auto before = world.state_digest();
    std::string error;
    const iag::Mutation condition{
        iag::MutationKind::SetMachineCondition, {"M-12"}, {}, 8.7, 68.0, 0.0, 30, "telemetry"};
    check(world.commit(0, {condition}, error), "condition transaction commits");
    check(world.revision() == 1, "commit increments revision");
    check(world.state().machines.at({"M-12"}).state == iag::MachineState::Degraded,
          "condition derives degraded state");
    check(before != world.state_digest(), "world digest changes after commit");

    const auto failed_digest = world.state_digest();
    const iag::Mutation invalid{
        iag::MutationKind::AssignWorkOrderLine, {"WO-1048"}, {"LINE-B"},
        999.0, 0.0, 999.0, 0, "invalid assignment"};
    check(!world.commit(1, {invalid}, error), "invalid transaction is rejected");
    check(world.revision() == 1, "failed transaction does not increment revision");
    check(world.state_digest() == failed_digest,
          "failed transaction is atomic and leaves state unchanged");
    check(!world.commit(0, {condition}, error), "stale revision is rejected");
    check(error.find("STALE_REVISION") == 0, "stale revision has explicit error");
    check(world.events().size() == 1, "journal records committed event");

    std::cout << "world foundation tests passed\n";
}

