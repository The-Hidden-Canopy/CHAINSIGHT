#include "iag/operations.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace iag {

namespace {

std::string quote(std::string_view text) {
    std::string result{"\""};
    for (const char ch : text) {
        switch (ch) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += ch; break;
        }
    }
    result += '"';
    return result;
}

std::string number(double value) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << value;
    return output.str();
}

double plan_score(const PlanCandidate& candidate) {
    const auto& simulation = candidate.simulation;
    return simulation.on_time_probability * 100.0 -
           simulation.expected_lateness_minutes * 0.2 -
           simulation.unplanned_downtime_minutes * 0.1 -
           simulation.energy_cost * 0.1 -
           simulation.maintenance_cost * 0.02;
}

std::string steps_json(const std::vector<PlanStep>& steps) {
    std::string result{"["};
    for (std::size_t index = 0; index < steps.size(); ++index) {
        if (index > 0) result += ',';
        const auto& step = steps[index];
        result += "{\"id\":" + quote(step.id) + ",\"action\":" +
                  quote(step.action) + ",\"subject\":" + quote(step.subject.value) +
                  ",\"destination\":" + quote(step.destination.value) +
                  ",\"quantity\":" + number(step.quantity) + "}";
    }
    result += ']';
    return result;
}

std::string violations_json(const std::vector<std::string>& violations) {
    std::string result{"["};
    for (std::size_t index = 0; index < violations.size(); ++index) {
        if (index > 0) result += ',';
        result += quote(violations[index]);
    }
    result += ']';
    return result;
}

std::string simulation_json(const SimulationResult& simulation) {
    return "{\"on_time_probability\":" + number(simulation.on_time_probability) +
           ",\"expected_lateness_minutes\":" +
           number(simulation.expected_lateness_minutes) +
           ",\"unplanned_downtime_minutes\":" +
           number(simulation.unplanned_downtime_minutes) +
           ",\"maintenance_cost\":" + number(simulation.maintenance_cost) +
           ",\"inventory_shortfall\":" +
           number(simulation.inventory_shortfall) +
           ",\"warehouse_delay_minutes\":" +
           number(simulation.warehouse_delay_minutes) +
           ",\"transport_delay_minutes\":" +
           number(simulation.transport_delay_minutes) +
           ",\"energy_cost\":" + number(simulation.energy_cost) +
           ",\"labor_utilization\":" +
           number(simulation.labor_utilization) +
           ",\"consequence_summary\":" +
           quote(simulation.consequence_summary) + "}";
}

std::vector<std::string> split_pipe(std::string_view value) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find('|', start);
        parts.emplace_back(value.substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return parts;
}

bool parse_bool(std::string_view value, bool& result) {
    if (value == "1") {
        result = true;
        return true;
    }
    if (value == "0") {
        result = false;
        return true;
    }
    return false;
}

bool parse_unsigned(std::string_view value, std::uint64_t& result) {
    try {
        std::size_t used = 0;
        result = std::stoull(std::string(value), &used);
        return used == value.size();
    } catch (...) {
        result = 0;
        return false;
    }
}

}  // namespace

PlanningResult generate_plan_candidates(const IndustrialWorld& world,
                                        const std::vector<Finding>& findings) {
    const auto& state = world.state();
    const auto& order = state.work_orders.at(ObjectId{"WO-1048"});
    const auto& machine = state.machines.at(ObjectId{"M-12"});
    const auto& tariff = state.tariffs.at(ObjectId{"SITE-01"});
    const auto& vehicle = state.vehicles.at(ObjectId{"T-7"});
    const double remaining = order.quantity_required - order.quantity_complete;
    const double peak_energy = tariff.multiplier;
    const double forecast_risk = findings.empty() ? machine.degradation_index
                                                   : findings.back().value;

    PlanningResult result;
    result.candidates.push_back(PlanCandidate{
        "A",
        "continue Line A until maintenance window",
        {PlanStep{"step_A_continue", "CONTINUE_PRODUCTION", {"WO-1048"},
                  {"LINE-A"}, remaining, {}}},
        SimulationResult{
            0.42, 180.0, 120.0, 800.0, 0.0, 45.0,
            static_cast<double>(vehicle.delay_minutes), remaining * peak_energy,
            0.55,
            "Lowest changeover cost, but degradation creates a high lateness tail."},
        true,
        {}});
    result.candidates.push_back(PlanCandidate{
        "B",
        "move all remaining work to Line B",
        {PlanStep{"step_B_move_all", "MOVE_WORK_ORDER", {"WO-1048"},
                  {"LINE-B"}, remaining, {}}},
        SimulationResult{
            0.96, 0.0, 0.0, 1200.0, 0.0, 45.0,
            static_cast<double>(vehicle.delay_minutes), remaining * peak_energy,
            0.86,
            "Likely on-time, but exposes the peak tariff and consumes the full changeover window."},
        true,
        {}});
    result.candidates.push_back(PlanCandidate{
        "C",
        "split production and start planned maintenance",
        {PlanStep{"step_C_move_partial", "MOVE_WORK_ORDER", {"WO-1048"},
                  {"LINE-B"}, 100.0, {}},
         PlanStep{"step_C_maintenance", "CREATE_MAINTENANCE_TASK", {"MT-1048"},
                  {"M-12"}, 0.0, {"step_C_move_partial"}}},
        SimulationResult{
            0.91, 20.0, 30.0, 800.0, 0.0, 45.0,
            static_cast<double>(vehicle.delay_minutes), 60.0 + 100.0 * peak_energy,
            0.72,
            "Balanced risk: limits Line A exposure, avoids the full peak move, and preserves a qualified repair window."},
        true,
        {}});
    result.candidates.push_back(PlanCandidate{
        "D",
        "move an unqualified quantity to Line B",
        {PlanStep{"step_D_move_too_much", "MOVE_WORK_ORDER", {"WO-1048"},
                  {"LINE-B"}, 220.0, {}}},
        SimulationResult{
            0.99, 0.0, 0.0, 1500.0, 0.0, 45.0,
            static_cast<double>(vehicle.delay_minutes), 220.0 * peak_energy,
            0.95,
            "Nominally fast, but violates the Line B fixture qualification envelope."},
        false,
        {"FIXTURE_NOT_QUALIFIED:LINE-B quantity > 180"}});

    double best_score = -1.0e18;
    for (const auto& candidate : result.candidates) {
        if (!candidate.hard_constraints_ok) continue;
        if (candidate.id == "A" && forecast_risk > 0.70) continue;
        const auto score = plan_score(candidate);
        if (score > best_score) {
            best_score = score;
            result.selected_candidate_id = candidate.id;
        }
    }
    if (result.selected_candidate_id.empty()) {
        result.selected_candidate_id = "C";
    }
    return result;
}

std::string candidates_json(const PlanningResult& result) {
    std::string output{"{\n  \"selected_candidate\":" +
                       quote(result.selected_candidate_id) +
                       ",\n  \"candidates\":["};
    for (std::size_t index = 0; index < result.candidates.size(); ++index) {
        if (index > 0) output += ',';
        const auto& candidate = result.candidates[index];
        output += "{\"id\":" + quote(candidate.id) +
                  ",\"strategy\":" + quote(candidate.strategy) +
                  ",\"hard_constraints_ok\":" +
                  (candidate.hard_constraints_ok ? "true" : "false") +
                  ",\"violations\":" + violations_json(candidate.violations) +
                  ",\"steps\":" + steps_json(candidate.steps) + "}";
    }
    output += "]\n}\n";
    return output;
}

std::string simulations_json(const PlanningResult& result) {
    std::string output{"{\n  \"simulations\":["};
    for (std::size_t index = 0; index < result.candidates.size(); ++index) {
        if (index > 0) output += ',';
        const auto& candidate = result.candidates[index];
        output += "{\"candidate_id\":" + quote(candidate.id) +
                  ",\"simulation\":" + simulation_json(candidate.simulation) + "}";
    }
    output += "]\n}\n";
    return output;
}

std::string to_string(GovernanceAction action) {
    switch (action) {
        case GovernanceAction::Allow: return "ALLOW";
        case GovernanceAction::Limit: return "LIMIT";
        case GovernanceAction::RequireApproval: return "REQUIRE_APPROVAL";
        case GovernanceAction::Deny: return "DENY";
    }
    return "DENY";
}

GovernanceDecision govern(const IndustrialWorld& world,
                          const PlanCandidate& candidate,
                          const ActionRequest& request) {
    (void)world;
    GovernanceDecision decision;
    decision.id = "GD-" + request.id;
    decision.policy_epoch = "policy.reschedule.production@17";
    if (!candidate.hard_constraints_ok) {
        decision.action = GovernanceAction::Deny;
        decision.reasons.push_back("hard constraint violation");
    } else if (request.action != "MOVE_WORK_ORDER") {
        decision.action = GovernanceAction::Deny;
        decision.reasons.push_back("unsupported action vocabulary");
    } else if (request.quantity > 180.0) {
        decision.action = GovernanceAction::Deny;
        decision.reasons.push_back("fixture qualification limit exceeded");
    } else if (candidate.id == "B") {
        decision.action = GovernanceAction::RequireApproval;
        decision.reasons.push_back("full move crosses peak energy exposure policy");
    } else if (candidate.id == "A") {
        decision.action = GovernanceAction::RequireApproval;
        decision.reasons.push_back("degradation risk requires operator approval");
    } else {
        decision.action = GovernanceAction::Allow;
        decision.reasons.push_back("bounded split move is within the site envelope");
        decision.reasons.push_back("one-step execution and verification required");
    }
    std::string canonical = decision.id + "|" + to_string(decision.action) + "|" +
                            decision.policy_epoch + "|" + request.candidate_id + "|" +
                            std::to_string(request.quantity);
    for (const auto& reason : decision.reasons) canonical += "|" + reason;
    decision.decision_hash = digest(canonical);
    return decision;
}

std::string decision_json(const GovernanceDecision& decision,
                          const ActionRequest& request) {
    std::string output{"{\n  \"id\":"};
    output += quote(decision.id);
    output += ",\n  \"action\":" + quote(to_string(decision.action));
    output += ",\n  \"policy_epoch\":" + quote(decision.policy_epoch);
    output += ",\n  \"decision_hash\":" + quote(decision.decision_hash);
    output += ",\n  \"request\":{\"id\":" + quote(request.id);
    output += ",\"actor\":" + quote(request.actor);
    output += ",\"action\":" + quote(request.action);
    output += ",\"candidate_id\":" + quote(request.candidate_id);
    output += ",\"subject\":" + quote(request.subject.value);
    output += ",\"destination\":" + quote(request.destination.value);
    output += ",\"quantity\":" + number(request.quantity);
    output += ",\"expected_revision\":" +
              std::to_string(request.expected_revision);
    output += "},\n  \"reasons\":[";
    for (std::size_t index = 0; index < decision.reasons.size(); ++index) {
        if (index > 0) output += ',';
        output += quote(decision.reasons[index]);
    }
    output += "]\n}\n";
    return output;
}

ExecutionResult SimulatedExecutionAdapter::dispatch(
    IndustrialWorld& world,
    const GovernanceDecision& decision,
    const ActionRequest& request) {
    const std::string idempotency_key =
        digest(request.action + "|" + request.candidate_id + "|" +
               request.subject.value + "|" + request.destination.value + "|" +
               std::to_string(request.quantity));
    if (const auto existing = receipts_.find(idempotency_key);
        existing != receipts_.end()) {
        auto replay = existing->second;
        replay.status = "IDEMPOTENT_REPLAY";
        return replay;
    }

    ExecutionResult result;
    result.idempotency_key = idempotency_key;
    result.before_revision = world.revision();
    result.after_revision = world.revision();
    if (decision.action != GovernanceAction::Allow &&
        decision.action != GovernanceAction::Limit) {
        result.status = "BLOCKED_BY_GOVERNANCE";
        result.detail = "No external adapter call was permitted.";
        receipts_.emplace(idempotency_key, result);
        return result;
    }

    if (request.action != "MOVE_WORK_ORDER") {
        result.status = "UNSUPPORTED_ACTION";
        result.detail = "The simulated adapter rejects unknown actions.";
        receipts_.emplace(idempotency_key, result);
        return result;
    }

    ++external_call_count_;
    const Mutation mutation{MutationKind::AssignWorkOrderLine, request.subject,
                            request.destination, 0.0, 0.0, request.quantity, 0,
                            request.id};
    std::string error;
    if (!world.commit(request.expected_revision, {mutation}, error)) {
        result.status = "COMMIT_REJECTED";
        result.detail = error;
        receipts_.emplace(idempotency_key, result);
        return result;
    }

    result.dispatched = true;
    result.after_revision = world.revision();
    const auto& order = world.state().work_orders.at(request.subject);
    result.verified = order.assigned_line == request.destination &&
                      order.scheduled_quantity == request.quantity;
    if (verification_override_enabled_) {
        result.verified = verification_override_;
    }
    result.replan_required = result.dispatched && !result.verified;
    result.status = result.verified ? "VERIFIED" : "VERIFY_MISMATCH";
    result.detail = result.verified
                        ? "SimMES observed the requested bounded assignment."
                        : "SimMES returned a state that did not match the request.";
    receipts_.emplace(idempotency_key, result);
    return result;
}

std::string SimulatedExecutionAdapter::receipts_text() const {
    std::string result{"receipt_store: 1\n"};
    for (const auto& [key, receipt] : receipts_) {
        result += "receipt|" + key + "|" + (receipt.dispatched ? "1" : "0") +
                  "|" + (receipt.verified ? "1" : "0") + "|" +
                  (receipt.replan_required ? "1" : "0") + "|" +
                  std::to_string(receipt.before_revision) + "|" +
                  std::to_string(receipt.after_revision) + "|" + receipt.status +
                  "|" + receipt.detail + "\n";
    }
    return result;
}

bool SimulatedExecutionAdapter::save_receipts(std::string_view path,
                                              std::string& error) const {
    std::ofstream output{std::string(path), std::ios::binary};
    if (!output) {
        error = "RECEIPT_STORE_NOT_WRITABLE:" + std::string(path);
        return false;
    }
    output << receipts_text();
    if (!output) {
        error = "RECEIPT_STORE_WRITE_FAILED:" + std::string(path);
        return false;
    }
    return true;
}

bool SimulatedExecutionAdapter::load_receipts(std::string_view path,
                                              std::string& error) {
    std::ifstream input{std::string(path), std::ios::binary};
    if (!input) {
        error = "RECEIPT_STORE_NOT_FOUND:" + std::string(path);
        return false;
    }
    receipts_.clear();
    std::string line;
    bool header_seen = false;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        if (line == "receipt_store: 1") {
            header_seen = true;
            continue;
        }
        const auto parts = split_pipe(line);
        if (parts.size() < 9 || parts[0] != "receipt") {
            error = "INVALID_RECEIPT_STORE_RECORD";
            return false;
        }
        bool dispatched = false;
        bool verified = false;
        bool replan_required = false;
        if (!parse_bool(parts[2], dispatched) ||
            !parse_bool(parts[3], verified) ||
            !parse_bool(parts[4], replan_required)) {
            error = "INVALID_RECEIPT_STORE_FLAGS";
            return false;
        }
        std::uint64_t before_revision = 0;
        std::uint64_t after_revision = 0;
        if (!parse_unsigned(parts[5], before_revision) ||
            !parse_unsigned(parts[6], after_revision)) {
            error = "INVALID_RECEIPT_STORE_REVISION";
            return false;
        }
        receipts_[parts[1]] = ExecutionResult{
            parts[1], dispatched, verified, replan_required, before_revision,
            after_revision, parts[7], parts[8]};
    }
    if (!header_seen) {
        error = "INVALID_RECEIPT_STORE_HEADER";
        return false;
    }
    return true;
}

std::string execution_json(const ExecutionResult& result) {
    return "{\n  \"idempotency_key\":" + quote(result.idempotency_key) +
           ",\n  \"dispatched\":" + (result.dispatched ? "true" : "false") +
           ",\n  \"verified\":" + (result.verified ? "true" : "false") +
           ",\n  \"replan_required\":" +
           (result.replan_required ? "true" : "false") +
           ",\n  \"before_revision\":" + std::to_string(result.before_revision) +
           ",\n  \"after_revision\":" + std::to_string(result.after_revision) +
           ",\n  \"status\":" + quote(result.status) +
           ",\n  \"detail\":" + quote(result.detail) + "\n}\n";
}

std::string replay_manifest_json(
    const IndustrialWorld& before,
    const IndustrialWorld& after,
    const ScenarioDefinition& scenario,
    const std::vector<EvidenceItem>& evidence,
    const std::vector<Finding>& findings,
    const PlanningResult& planning,
    const GovernanceDecision& decision,
    const ExecutionResult& execution) {
    const std::string replay_key =
        digest(before.state_digest() + "|" + after.state_digest() + "|" +
               std::to_string(scenario.seed) + "|" + decision.decision_hash +
               "|" + execution.status);
    return "{\n  \"scenario\":" + quote(scenario.name) +
           ",\n  \"seed\":" + std::to_string(scenario.seed) +
           ",\n  \"initial_revision\":" + std::to_string(before.revision()) +
           ",\n  \"final_revision\":" + std::to_string(after.revision()) +
           ",\n  \"initial_digest\":" + quote(before.state_digest()) +
           ",\n  \"final_digest\":" + quote(after.state_digest()) +
           ",\n  \"evidence_count\":" + std::to_string(evidence.size()) +
           ",\n  \"finding_count\":" + std::to_string(findings.size()) +
           ",\n  \"candidate_count\":" +
           std::to_string(planning.candidates.size()) +
           ",\n  \"decision_hash\":" + quote(decision.decision_hash) +
           ",\n  \"execution_status\":" + quote(execution.status) +
           ",\n  \"replay_digest\":" + quote(replay_key) + "\n}\n";
}

std::string report_markdown(
    const IndustrialWorld& before,
    const IndustrialWorld& after,
    const ScenarioDefinition& scenario,
    const std::vector<EvidenceItem>& evidence,
    const std::vector<Finding>& findings,
    const PlanningResult& planning,
    const GovernanceDecision& decision,
    const ExecutionResult& execution) {
    const auto selected = std::find_if(
        planning.candidates.begin(), planning.candidates.end(),
        [&](const PlanCandidate& candidate) {
            return candidate.id == planning.selected_candidate_id;
        });
    std::string report = "# CHAINSIGHT scenario report\n\n";
    report += "- Scenario: " + scenario.name + "\n";
    report += "- Seed: " + std::to_string(scenario.seed) + "\n";
    report += "- Initial revision/digest: " + std::to_string(before.revision()) +
              " / " + before.state_digest() + "\n";
    report += "- Final revision/digest: " + std::to_string(after.revision()) +
              " / " + after.state_digest() + "\n";
    report += "- Evidence admitted: " + std::to_string(evidence.size()) + "\n";
    report += "- Findings derived: " + std::to_string(findings.size()) + "\n\n";
    report += "## Candidate comparison\n\n";
    report += "| Candidate | Hard constraints | On-time probability | Lateness | Energy | Decision |\n";
    report += "|---|---:|---:|---:|---:|---|\n";
    for (const auto& candidate : planning.candidates) {
        report += "|" + candidate.id + " " + candidate.strategy + "|" +
                  (candidate.hard_constraints_ok ? "yes" : "no") + "|" +
                  number(candidate.simulation.on_time_probability) + "|" +
                  number(candidate.simulation.expected_lateness_minutes) + "|" +
                  number(candidate.simulation.energy_cost) + "|" +
                  (candidate.id == planning.selected_candidate_id ? "selected" : "not selected") +
                  "|\n";
    }
    report += "\n## Governance and execution\n\n";
    report += "- Selected candidate: " + planning.selected_candidate_id + "\n";
    report += "- Governance: " + to_string(decision.action) + "\n";
    report += "- Decision hash: " + decision.decision_hash + "\n";
    report += "- Execution: " + execution.status + "\n";
    report += "- Verified: " + std::string(execution.verified ? "yes" : "no") + "\n";
    report += "- Replan trigger: world revision changed from " +
              std::to_string(execution.before_revision) + " to " +
              std::to_string(execution.after_revision) + "\n\n";
    if (selected != planning.candidates.end()) {
        report += "## Consequence summary\n\n" +
                  selected->simulation.consequence_summary + "\n";
    }
    report += "\nAll values are deterministic simulation outputs. They are not observed plant truth.\n";
    return report;
}

}  // namespace iag
