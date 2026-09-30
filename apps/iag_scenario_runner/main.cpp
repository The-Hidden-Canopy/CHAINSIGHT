#include "iag/operations.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output << content;
    return static_cast<bool>(output);
}

const iag::PlanCandidate* find_candidate(const iag::PlanningResult& planning,
                                         const std::string& id) {
    for (const auto& candidate : planning.candidates) {
        if (candidate.id == id) return &candidate;
    }
    return nullptr;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: iag_scenario_runner <scenario.yaml> [--output <dir>]\n";
        return 2;
    }

    std::filesystem::path scenario_path{argv[1]};
    std::filesystem::path output_dir{"artifacts/bearing_failure_compound"};
    for (int index = 2; index + 1 < argc; ++index) {
        if (std::string{argv[index]} == "--output") {
            output_dir = argv[++index];
        }
    }
    std::filesystem::create_directories(output_dir);

    std::string error;
    const auto scenario =
        iag::load_scenario_file(scenario_path.string(), error);
    if (!error.empty()) {
        std::cerr << error << '\n';
        return 1;
    }

    auto world = iag::IndustrialWorld::synthetic_enterprise();
    const auto before = world;
    if (!write_file(output_dir / "world_before.json", before.to_json())) {
        std::cerr << "unable to write world_before.json\n";
        return 1;
    }

    std::vector<iag::EvidenceItem> evidence;
    std::vector<iag::Finding> findings;
    if (!iag::run_scenario_events(world, scenario, evidence, findings, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (!write_file(output_dir / "evidence.jsonl",
                    iag::evidence_jsonl(evidence)) ||
        !write_file(output_dir / "findings.jsonl",
                    iag::findings_jsonl(findings))) {
        std::cerr << "unable to write evidence artifacts\n";
        return 1;
    }

    const auto planning = iag::generate_plan_candidates(world, findings);
    if (!write_file(output_dir / "candidates.json",
                    iag::candidates_json(planning)) ||
        !write_file(output_dir / "simulation_results.json",
                    iag::simulations_json(planning))) {
        std::cerr << "unable to write planning artifacts\n";
        return 1;
    }

    const auto* selected =
        find_candidate(planning, planning.selected_candidate_id);
    if (selected == nullptr || selected->steps.empty()) {
        std::cerr << "planner returned no executable candidate\n";
        return 1;
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
    if (!write_file(output_dir / "decision.json",
                    iag::decision_json(decision, request))) {
        std::cerr << "unable to write decision artifact\n";
        return 1;
    }

    iag::SimulatedExecutionAdapter adapter;
    const auto execution = adapter.dispatch(world, decision, request);
    const auto idempotent_replay = adapter.dispatch(world, decision, request);
    if (execution.dispatched && idempotent_replay.status != "IDEMPOTENT_REPLAY") {
        std::cerr << "idempotency check failed\n";
        return 1;
    }
    if (!write_file(output_dir / "execution_receipt.json",
                    iag::execution_json(execution))) {
        std::cerr << "unable to write execution artifact\n";
        return 1;
    }

    const auto after = world;
    if (!write_file(output_dir / "world_after.json", after.to_json()) ||
        !write_file(output_dir / "replay_manifest.json",
                    iag::replay_manifest_json(
                        before, after, scenario, evidence, findings, planning,
                        decision, execution)) ||
        !write_file(output_dir / "report.md",
                    iag::report_markdown(
                        before, after, scenario, evidence, findings, planning,
                        decision, execution))) {
        std::cerr << "unable to write reconstruction artifacts\n";
        return 1;
    }

    std::cout << "scenario=" << scenario.name << '\n'
              << "initial_revision=" << before.revision() << '\n'
              << "final_revision=" << after.revision() << '\n'
              << "selected_candidate=" << planning.selected_candidate_id << '\n'
              << "governance=" << iag::to_string(decision.action) << '\n'
              << "execution=" << execution.status << '\n'
              << "final_digest=" << after.state_digest() << '\n'
              << "artifacts=" << output_dir.string() << '\n';
    return execution.verified ? 0 : 1;
}

