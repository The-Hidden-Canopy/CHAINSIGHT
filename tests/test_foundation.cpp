#include "iag/iag.hpp"
#include "iag/operations.hpp"

#include <cstdlib>
#include <filesystem>
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

    std::string scenario_error;
    const auto scenario_path =
        std::filesystem::path(__FILE__).parent_path().parent_path() /
        "scenarios" / "bearing_failure_compound.yaml";
    const auto scenario =
        iag::load_scenario_file(scenario_path.string(), scenario_error);
    check(scenario_error.empty(), "canonical scenario parses");
    check(scenario.name == "bearing_failure_compound",
          "scenario name is preserved");
    check(scenario.seed == 99117, "scenario seed is preserved");
    check(scenario.events.size() == 4, "scenario has four deterministic events");
    check(scenario.events.at(1).at == 30, "scenario time is parsed");

    auto scenario_world = iag::IndustrialWorld::synthetic_enterprise();
    std::vector<iag::EvidenceItem> evidence;
    std::vector<iag::Finding> findings;
    check(iag::run_scenario_events(scenario_world, scenario, evidence, findings,
                                   scenario_error),
          "scenario events admit");
    check(evidence.size() == 4, "all scenario events become evidence");
    check(findings.size() == 2, "trend and forecast findings are derived");
    check(scenario_world.state().vehicles.at({"T-7"}).delay_minutes == 45,
          "traffic evidence updates vehicle belief");
    check(scenario_world.state().tariffs.at({"SITE-01"}).multiplier == 2.2,
          "energy evidence updates tariff belief");
    check(evidence.front().quality == iag::Quality::Valid,
          "evidence quality is explicit");
    check(iag::evidence_jsonl(evidence).find("\"sequence\":1") !=
              std::string::npos,
          "evidence output preserves sequence");

    const auto planning =
        iag::generate_plan_candidates(scenario_world, findings);
    check(planning.candidates.size() == 4, "planner generates alternatives");
    check(planning.selected_candidate_id == "C",
          "planner selects the balanced candidate");
    check(!planning.candidates.at(3).hard_constraints_ok,
          "hard constraint rejection is explicit");
    check(planning.candidates.at(3).violations.at(0).find("FIXTURE_NOT_QUALIFIED") ==
              0,
          "rejected candidate preserves constraint reason");

    const auto& selected = planning.candidates.at(2);
    const auto& step = selected.steps.front();
    const iag::ActionRequest request{
        "AR-C", "planner", step.action, selected.id, step.subject,
        step.destination, step.quantity, scenario_world.revision(),
        "contain degradation"};
    const auto decision = iag::govern(scenario_world, selected, request);
    check(decision.action == iag::GovernanceAction::Allow,
          "bounded split move is allowed");
    iag::SimulatedExecutionAdapter adapter;
    const auto execution = adapter.dispatch(scenario_world, decision, request);
    check(execution.dispatched && execution.verified,
          "simulated execution is verified");
    check(scenario_world.revision() == 5,
          "execution advances the world after four evidence commits");
    const auto replay = adapter.dispatch(scenario_world, decision, request);
    check(replay.status == "IDEMPOTENT_REPLAY",
          "repeated action is idempotent");

    const auto denied_decision =
        iag::govern(scenario_world, planning.candidates.at(3), request);
    check(denied_decision.action == iag::GovernanceAction::Deny,
          "denied candidate cannot be governed");

    std::cout << "world, evidence, planning, and execution tests passed\n";
}
