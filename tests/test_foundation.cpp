#include "iag/iag.hpp"
#include "iag/operations.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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
    const auto fixture_path =
        std::filesystem::path(__FILE__).parent_path().parent_path() /
        "fixtures" / "enterprise_v1.yaml";
    std::string fixture_error;
    const auto fixture_world =
        iag::IndustrialWorld::load_fixture_file(fixture_path.string(), fixture_error);
    check(fixture_error.empty(), "documented enterprise fixture loads");
    check(fixture_world.state_digest() == world.state_digest(),
          "fixture reproduces the built-in enterprise digest");
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

    const auto snapshot_path =
        std::filesystem::temp_directory_path() / "chainsight-world.snapshot";
    {
        std::ofstream snapshot(snapshot_path, std::ios::binary);
        snapshot << world.snapshot_text();
    }
    std::string snapshot_error;
    const auto restored =
        iag::IndustrialWorld::load_snapshot_file(snapshot_path.string(), snapshot_error);
    check(snapshot_error.empty(), "world snapshot reloads");
    check(restored.revision() == world.revision(),
          "snapshot restores world revision");
    check(restored.state_digest() == world.state_digest(),
          "snapshot restores exact state digest");
    check(restored.events().size() == world.events().size(),
          "snapshot restores committed journal");
    std::filesystem::remove(snapshot_path);

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

    auto scenario_world = fixture_world;
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
    check(evidence.at(0).source_sequence == 1 &&
              evidence.at(1).source_sequence == 2,
          "source sequences are tracked independently");
    check(iag::evidence_jsonl(evidence).find("\"sequence\":1") !=
              std::string::npos,
          "evidence output preserves sequence");

    auto rejected_quality_world = fixture_world;
    auto rejected_quality_scenario = scenario;
    rejected_quality_scenario.events.at(1).quality = iag::Quality::Stale;
    std::vector<iag::EvidenceItem> rejected_quality_evidence;
    std::vector<iag::Finding> rejected_quality_findings;
    std::string rejected_quality_error;
    check(!iag::run_scenario_events(
              rejected_quality_world, rejected_quality_scenario,
              rejected_quality_evidence, rejected_quality_findings,
              rejected_quality_error),
          "stale evidence is rejected");
    check(rejected_quality_error.find("EVIDENCE_QUALITY_BLOCKED") == 0,
          "stale evidence has an explicit quality error");
    check(rejected_quality_world.revision() == fixture_world.revision(),
          "quality rejection is atomic");

    auto contradictory_scenario = scenario;
    contradictory_scenario.events = {
        iag::ScenarioEvent{0, iag::EvidenceKind::MachineCondition, {"M-12"},
                           "sim.telemetry", 1, iag::Quality::Valid, 6.1, 0.0},
        iag::ScenarioEvent{0, iag::EvidenceKind::MachineCondition, {"M-12"},
                           "sim.maintenance", 1, iag::Quality::Valid, 7.1, 0.0}};
    auto contradictory_world = fixture_world;
    std::vector<iag::EvidenceItem> contradictory_evidence;
    std::vector<iag::Finding> contradictory_findings;
    std::string contradictory_error;
    check(!iag::run_scenario_events(
              contradictory_world, contradictory_scenario,
              contradictory_evidence, contradictory_findings,
              contradictory_error),
          "contradictory evidence is rejected");
    check(contradictory_error.find("EVIDENCE_CONTRADICTION") == 0,
          "contradiction has an explicit error");
    check(contradictory_world.revision() == fixture_world.revision(),
          "contradiction rejection is atomic");

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

    const auto receipt_path =
        std::filesystem::temp_directory_path() / "chainsight-receipts.store";
    std::string receipt_error;
    check(adapter.save_receipts(receipt_path.string(), receipt_error),
          "execution receipts persist");
    iag::SimulatedExecutionAdapter restarted_adapter;
    check(restarted_adapter.load_receipts(receipt_path.string(), receipt_error),
          "execution receipts reload");
    const auto restarted_replay =
        restarted_adapter.dispatch(scenario_world, decision, request);
    check(restarted_replay.status == "IDEMPOTENT_REPLAY",
          "restart does not duplicate external action");
    std::filesystem::remove(receipt_path);

    const auto denied_decision =
        iag::govern(scenario_world, planning.candidates.at(3), request);
    check(denied_decision.action == iag::GovernanceAction::Deny,
          "denied candidate cannot be governed");
    const auto denied_execution =
        adapter.dispatch(scenario_world, denied_decision, request);
    check(denied_execution.status == "BLOCKED_BY_GOVERNANCE",
          "denied action is blocked before adapter execution");
    check(adapter.external_call_count() == 1,
          "denied action does not reach the external adapter");

    auto mismatch_world = fixture_world;
    std::vector<iag::EvidenceItem> mismatch_evidence;
    std::vector<iag::Finding> mismatch_findings;
    std::string mismatch_error;
    check(iag::run_scenario_events(mismatch_world, scenario, mismatch_evidence,
                                   mismatch_findings, mismatch_error),
          "mismatch scenario admits");
    const auto mismatch_planning =
        iag::generate_plan_candidates(mismatch_world, mismatch_findings);
    const auto& mismatch_candidate = mismatch_planning.candidates.at(2);
    const auto& mismatch_step = mismatch_candidate.steps.front();
    const iag::ActionRequest mismatch_request{
        "AR-MISMATCH", "test", mismatch_step.action, mismatch_candidate.id,
        mismatch_step.subject, mismatch_step.destination, mismatch_step.quantity,
        mismatch_world.revision(), "verify mismatch"};
    const auto mismatch_decision =
        iag::govern(mismatch_world, mismatch_candidate, mismatch_request);
    iag::SimulatedExecutionAdapter mismatch_adapter;
    mismatch_adapter.set_verification_override(false);
    const auto mismatch_execution =
        mismatch_adapter.dispatch(mismatch_world, mismatch_decision,
                                  mismatch_request);
    check(mismatch_execution.dispatched && !mismatch_execution.verified,
          "verification mismatch is observable");
    check(mismatch_execution.replan_required &&
              mismatch_execution.status == "VERIFY_MISMATCH",
          "verification mismatch requires replan");

    std::cout << "world, evidence, planning, and execution tests passed\n";
}
