#include "iag/operations.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

struct RunSummary {
    std::string scenario;
    iag::Revision initial_revision{0};
    iag::Revision final_revision{0};
    std::string initial_digest;
    std::string final_digest;
    std::string world_before;
    std::string evidence;
    std::string findings;
    std::string candidates;
    std::string simulations;
    std::string decision;
    std::string execution;
    std::string world_after;
    std::string replay_manifest;
    std::string report;
    bool verified{false};
};

bool write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output << content;
    return static_cast<bool>(output);
}

std::string quote(std::string_view value) {
    std::string result{"\""};
    for (const auto ch : value) {
        if (ch == '\\' || ch == '"') result += '\\';
        result += ch;
    }
    result += '"';
    return result;
}

const iag::PlanCandidate* find_candidate(const iag::PlanningResult& planning,
                                         const std::string& id) {
    for (const auto& candidate : planning.candidates) {
        if (candidate.id == id) return &candidate;
    }
    return nullptr;
}

bool run_once(const std::filesystem::path& scenario_path,
              const std::filesystem::path& output_dir,
              RunSummary& summary,
              std::string& error) {
    std::filesystem::create_directories(output_dir);
    const auto scenario =
        iag::load_scenario_file(scenario_path.string(), error);
    if (!error.empty()) return false;

    const auto fixture_path = scenario_path.parent_path().parent_path() /
                              "fixtures" /
                              (scenario.world_fixture + ".yaml");
    auto world = iag::IndustrialWorld::load_fixture_file(
        fixture_path.string(), error);
    if (!error.empty()) return false;
    const auto before = world;
    summary.scenario = scenario.name;
    summary.initial_revision = before.revision();
    summary.initial_digest = before.state_digest();
    summary.world_before = before.to_json();
    if (!write_file(output_dir / "world_before.json", summary.world_before) ||
        !write_file(output_dir / "world_before.snapshot", before.snapshot_text())) {
        error = "UNABLE_TO_WRITE_WORLD_BEFORE";
        return false;
    }

    std::vector<iag::EvidenceItem> evidence;
    std::vector<iag::Finding> findings;
    if (!iag::run_scenario_events(world, scenario, evidence, findings, error)) {
        return false;
    }
    summary.evidence = iag::evidence_jsonl(evidence);
    summary.findings = iag::findings_jsonl(findings);
    if (!write_file(output_dir / "evidence.jsonl", summary.evidence) ||
        !write_file(output_dir / "findings.jsonl", summary.findings)) {
        error = "UNABLE_TO_WRITE_EVIDENCE_ARTIFACTS";
        return false;
    }

    const auto planning = iag::generate_plan_candidates(world, findings);
    summary.candidates = iag::candidates_json(planning);
    summary.simulations = iag::simulations_json(planning);
    if (!write_file(output_dir / "candidates.json", summary.candidates) ||
        !write_file(output_dir / "simulation_results.json", summary.simulations)) {
        error = "UNABLE_TO_WRITE_PLANNING_ARTIFACTS";
        return false;
    }

    const auto* selected =
        find_candidate(planning, planning.selected_candidate_id);
    if (selected == nullptr || selected->steps.empty()) {
        error = "PLANNER_RETURNED_NO_EXECUTABLE_CANDIDATE";
        return false;
    }
    const auto& step = selected->steps.front();
    const iag::ActionRequest request{
        "AR-" + selected->id,
        "planner",
        step.action,
        selected->id,
        step.subject,
        step.destination,
        step.quantity,
        world.revision(),
        "preserve the customer commitment while containing machine degradation"};
    const auto decision = iag::govern(world, *selected, request);
    summary.decision = iag::decision_json(decision, request);
    if (!write_file(output_dir / "decision.json", summary.decision)) {
        error = "UNABLE_TO_WRITE_DECISION_ARTIFACT";
        return false;
    }

    iag::SimulatedExecutionAdapter adapter;
    const auto execution = adapter.dispatch(world, decision, request);
    if (!adapter.save_receipts((output_dir / "execution_receipts.store").string(),
                               error)) {
        return false;
    }
    iag::SimulatedExecutionAdapter restarted_adapter;
    if (!restarted_adapter.load_receipts(
            (output_dir / "execution_receipts.store").string(), error)) {
        return false;
    }
    const auto idempotent_replay =
        restarted_adapter.dispatch(world, decision, request);
    if (execution.dispatched && idempotent_replay.status != "IDEMPOTENT_REPLAY") {
        error = "RESTART_IDEMPOTENCY_CHECK_FAILED";
        return false;
    }
    summary.execution = iag::execution_json(execution);
    if (!write_file(output_dir / "execution_receipt.json", summary.execution)) {
        error = "UNABLE_TO_WRITE_EXECUTION_ARTIFACT";
        return false;
    }

    const auto after = world;
    summary.final_revision = after.revision();
    summary.final_digest = after.state_digest();
    summary.world_after = after.to_json();
    summary.replay_manifest = iag::replay_manifest_json(
        before, after, scenario, evidence, findings, planning, decision, execution);
    summary.report = iag::report_markdown(
        before, after, scenario, evidence, findings, planning, decision, execution);
    if (!write_file(output_dir / "world_after.json", summary.world_after) ||
        !write_file(output_dir / "world_after.snapshot", after.snapshot_text()) ||
        !write_file(output_dir / "replay_manifest.json", summary.replay_manifest) ||
        !write_file(output_dir / "report.md", summary.report)) {
        error = "UNABLE_TO_WRITE_RECONSTRUCTION_ARTIFACTS";
        return false;
    }
    summary.verified = execution.verified;
    return summary.verified;
}

bool same_run(const RunSummary& first, const RunSummary& second) {
    return first.initial_revision == second.initial_revision &&
           first.final_revision == second.final_revision &&
           first.initial_digest == second.initial_digest &&
           first.final_digest == second.final_digest &&
           first.world_before == second.world_before &&
           first.evidence == second.evidence &&
           first.findings == second.findings &&
           first.candidates == second.candidates &&
           first.simulations == second.simulations &&
           first.decision == second.decision &&
           first.execution == second.execution &&
           first.world_after == second.world_after &&
           first.replay_manifest == second.replay_manifest &&
           first.report == second.report &&
           first.verified == second.verified;
}

bool write_replay_verification(const std::filesystem::path& output_dir,
                               const RunSummary& first,
                               const RunSummary& second,
                               bool matches) {
    const std::string content =
        std::string{"{\n  \"verified\":"} + (matches ? "true" : "false") +
        ",\n  \"initial_digest\":" + quote(first.initial_digest) +
        ",\n  \"replay_initial_digest\":" + quote(second.initial_digest) +
        ",\n  \"final_digest\":" + quote(first.final_digest) +
        ",\n  \"replay_final_digest\":" + quote(second.final_digest) +
        ",\n  \"final_revision\":" + std::to_string(first.final_revision) +
        ",\n  \"replay_final_revision\":" + std::to_string(second.final_revision) +
        "\n}\n";
    return write_file(output_dir / "replay_verification.json", content);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: iag_scenario_runner <scenario.yaml> [--output <dir>] [--no-replay-check]\n";
        return 2;
    }

    const std::filesystem::path scenario_path{argv[1]};
    std::filesystem::path output_dir{"artifacts/bearing_failure_compound"};
    bool replay_check = true;
    for (int index = 2; index < argc; ++index) {
        if (std::string{argv[index]} == "--output" && index + 1 < argc) {
            output_dir = argv[++index];
        } else if (std::string{argv[index]} == "--no-replay-check") {
            replay_check = false;
        }
    }

    RunSummary first;
    std::string error;
    if (!run_once(scenario_path, output_dir, first, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    if (replay_check) {
        RunSummary replay;
        const auto replay_dir = output_dir / "replay_run";
        if (!run_once(scenario_path, replay_dir, replay, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        const bool matches = same_run(first, replay);
        if (!write_replay_verification(output_dir, first, replay, matches)) {
            std::cerr << "UNABLE_TO_WRITE_REPLAY_VERIFICATION\n";
            return 1;
        }
        if (!matches) {
            std::cerr << "REPLAY_MISMATCH\n";
            return 1;
        }
    }

    std::cout << "scenario=" << first.scenario << '\n'
              << "initial_revision=" << first.initial_revision << '\n'
              << "final_revision=" << first.final_revision << '\n'
              << "final_digest=" << first.final_digest << '\n'
              << "replay_verified=" << (replay_check ? "true" : "skipped") << '\n'
              << "artifacts=" << output_dir.string() << '\n';
    return 0;
}
