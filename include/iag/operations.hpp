#pragma once

#include "iag/iag.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace iag {

struct PlanStep {
    std::string id;
    std::string action;
    ObjectId subject;
    ObjectId destination;
    double quantity{0.0};
    std::vector<std::string> depends_on;
};

struct SimulationResult {
    double on_time_probability{0.0};
    double expected_lateness_minutes{0.0};
    double unplanned_downtime_minutes{0.0};
    double maintenance_cost{0.0};
    double inventory_shortfall{0.0};
    double warehouse_delay_minutes{0.0};
    double transport_delay_minutes{0.0};
    double energy_cost{0.0};
    double labor_utilization{0.0};
    std::string consequence_summary;
};

struct PlanCandidate {
    std::string id;
    std::string strategy;
    std::vector<PlanStep> steps;
    SimulationResult simulation;
    bool hard_constraints_ok{false};
    std::vector<std::string> violations;
};

struct PlanningResult {
    std::vector<PlanCandidate> candidates;
    std::string selected_candidate_id;
};

PlanningResult generate_plan_candidates(const IndustrialWorld& world,
                                        const std::vector<Finding>& findings);
std::string candidates_json(const PlanningResult& result);
std::string simulations_json(const PlanningResult& result);

enum class GovernanceAction {
    Allow,
    Limit,
    RequireApproval,
    Deny,
};

std::string to_string(GovernanceAction action);

struct ActionRequest {
    std::string id;
    std::string actor;
    std::string action;
    std::string candidate_id;
    ObjectId subject;
    ObjectId destination;
    double quantity{0.0};
    Revision expected_revision{0};
    std::string purpose;
};

struct GovernanceDecision {
    std::string id;
    GovernanceAction action{GovernanceAction::Deny};
    std::string policy_epoch;
    std::vector<std::string> reasons;
    std::string decision_hash;
};

GovernanceDecision govern(const IndustrialWorld& world,
                          const PlanCandidate& candidate,
                          const ActionRequest& request);
std::string decision_json(const GovernanceDecision& decision,
                           const ActionRequest& request);

struct ExecutionResult {
    std::string idempotency_key;
    bool dispatched{false};
    bool verified{false};
    bool replan_required{false};
    Revision before_revision{0};
    Revision after_revision{0};
    std::string status;
    std::string detail;
};

class SimulatedExecutionAdapter {
public:
    ExecutionResult dispatch(IndustrialWorld& world,
                             const GovernanceDecision& decision,
                             const ActionRequest& request);

    std::string receipts_text() const;
    bool save_receipts(std::string_view path, std::string& error) const;
    bool load_receipts(std::string_view path, std::string& error);
    std::size_t external_call_count() const noexcept {
        return external_call_count_;
    }

    void set_verification_override(bool verified) noexcept {
        verification_override_enabled_ = true;
        verification_override_ = verified;
    }

private:
    std::map<std::string, ExecutionResult> receipts_;
    std::size_t external_call_count_{0};
    bool verification_override_enabled_{false};
    bool verification_override_{true};
};

std::string execution_json(const ExecutionResult& result);
std::string replay_manifest_json(const IndustrialWorld& before,
                                 const IndustrialWorld& after,
                                 const ScenarioDefinition& scenario,
                                 const std::vector<EvidenceItem>& evidence,
                                 const std::vector<Finding>& findings,
                                 const PlanningResult& planning,
                                 const GovernanceDecision& decision,
                                 const ExecutionResult& execution);
std::string report_markdown(const IndustrialWorld& before,
                            const IndustrialWorld& after,
                            const ScenarioDefinition& scenario,
                            const std::vector<EvidenceItem>& evidence,
                            const std::vector<Finding>& findings,
                            const PlanningResult& planning,
                            const GovernanceDecision& decision,
                            const ExecutionResult& execution);

}  // namespace iag
