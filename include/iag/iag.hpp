#pragma once

#include "iag/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace iag {

constexpr std::string_view version() noexcept {
    return "0.1.0";
}

enum class MachineState {
    Operational,
    Degraded,
    Failed,
    Maintenance,
    Offline,
};

enum class WorkOrderState {
    Scheduled,
    InProgress,
    Complete,
    Blocked,
};

enum class MaintenanceState {
    Planned,
    Assigned,
    InProgress,
    Complete,
};

std::string to_string(MachineState state);
std::string to_string(WorkOrderState state);
std::string to_string(MaintenanceState state);

struct Site {
    ObjectId id;
    std::string name;
};

struct ProductionLine {
    ObjectId id;
    ObjectId site;
    std::string name;
    double units_per_hour{0.0};
    bool available{true};
};

struct Machine {
    ObjectId id;
    ObjectId site;
    ObjectId line;
    MachineState state{MachineState::Operational};
    double vibration_rms{0.0};
    double temperature_c{0.0};
    double degradation_index{0.0};
};

struct Product {
    ObjectId id;
    std::string sku;
    ObjectId primary_line;
    ObjectId alternate_line;
};

struct WorkOrder {
    ObjectId id;
    ObjectId product;
    double quantity_required{0.0};
    double quantity_complete{0.0};
    double scheduled_quantity{0.0};
    SimMinute due_minute{0};
    WorkOrderState state{WorkOrderState::Scheduled};
    ObjectId assigned_line;
};

struct InventoryPosition {
    ObjectId id;
    ObjectId item;
    ObjectId location;
    double on_hand{0.0};
    double reserved{0.0};
    double quarantine{0.0};

    double available() const noexcept {
        return on_hand - reserved - quarantine;
    }
};

struct Warehouse {
    ObjectId id;
    ObjectId site;
    double capacity_units{0.0};
};

struct MaintenanceTask {
    ObjectId id;
    ObjectId machine;
    ObjectId technician;
    MaintenanceState state{MaintenanceState::Planned};
    SimMinute window_start{0};
    SimMinute window_end{0};
};

struct Technician {
    ObjectId id;
    ObjectId site;
    bool qualified{false};
    SimMinute available_from{0};
    SimMinute available_until{0};
};

struct Vehicle {
    ObjectId id;
    ObjectId route;
    SimMinute delay_minutes{0};
};

struct EnergyTariff {
    ObjectId site;
    double multiplier{1.0};
    SimMinute peak_start{0};
};

struct CustomerCommitment {
    ObjectId id;
    ObjectId work_order;
    SimMinute due_minute{0};
    bool hard_protected{false};
};

struct IndustrialState {
    SimMinute now{0};
    std::map<ObjectId, Site> sites;
    std::map<ObjectId, ProductionLine> lines;
    std::map<ObjectId, Machine> machines;
    std::map<ObjectId, Product> products;
    std::map<ObjectId, WorkOrder> work_orders;
    std::map<ObjectId, InventoryPosition> inventory;
    std::map<ObjectId, Warehouse> warehouses;
    std::map<ObjectId, MaintenanceTask> maintenance;
    std::map<ObjectId, Technician> technicians;
    std::map<ObjectId, Vehicle> vehicles;
    std::map<ObjectId, EnergyTariff> tariffs;
    std::map<ObjectId, CustomerCommitment> commitments;
};

enum class MutationKind {
    SetMachineCondition,
    AssignWorkOrderLine,
    CreateMaintenanceTask,
    SetTrafficDelay,
    SetEnergyMultiplier,
};

struct Mutation {
    MutationKind kind{MutationKind::SetMachineCondition};
    ObjectId subject;
    ObjectId destination;
    double value{0.0};
    double secondary_value{0.0};
    double quantity{0.0};
    SimMinute at{0};
    std::string note;
};

struct WorldEvent {
    Revision revision{0};
    std::string kind;
    ObjectId subject;
    std::string detail;
};

enum class EvidenceKind {
    MachineCondition,
    TrafficDelay,
    EnergyTariff,
};

struct EvidenceItem {
    std::string id;
    std::string source;
    EvidenceKind kind{EvidenceKind::MachineCondition};
    ObjectId subject;
    SimMinute observed_at{0};
    std::uint64_t sequence{0};
    double value{0.0};
    double secondary_value{0.0};
    Quality quality{Quality::Valid};
    std::string raw_hash;
};

struct Finding {
    std::string id;
    std::string kind;
    ObjectId subject;
    double value{0.0};
    double confidence{0.0};
    std::vector<std::string> evidence_ids;
    std::string explanation;
};

struct ScenarioEvent {
    SimMinute at{0};
    EvidenceKind kind{EvidenceKind::MachineCondition};
    ObjectId subject;
    double value{0.0};
    double secondary_value{0.0};
};

struct ScenarioDefinition {
    std::string name;
    std::uint64_t seed{0};
    std::string world_fixture{"enterprise_v1"};
    std::vector<ScenarioEvent> events;
};

class IndustrialWorld;

std::string to_string(EvidenceKind kind);
ScenarioDefinition load_scenario_file(std::string_view path, std::string& error);
bool run_scenario_events(IndustrialWorld& world,
                         const ScenarioDefinition& scenario,
                         std::vector<EvidenceItem>& evidence,
                         std::vector<Finding>& findings,
                         std::string& error);
std::string evidence_jsonl(const std::vector<EvidenceItem>& evidence);
std::string findings_jsonl(const std::vector<Finding>& findings);

class IndustrialWorld {
public:
    static IndustrialWorld synthetic_enterprise();
    static IndustrialWorld load_fixture_file(std::string_view path,
                                             std::string& error);
    static IndustrialWorld load_snapshot_file(std::string_view path,
                                              std::string& error);

    Revision revision() const noexcept { return revision_; }
    const IndustrialState& state() const noexcept { return state_; }
    std::vector<WorldEvent> events() const { return journal_; }

    std::string state_digest() const;
    std::string to_json() const;
    std::string snapshot_text() const;
    std::string events_json() const;

    bool commit(Revision expected_revision,
                const std::vector<Mutation>& mutations,
                std::string& error);

private:
    static bool apply(IndustrialState& state,
                      const Mutation& mutation,
                      std::string& error);

    IndustrialState state_;
    Revision revision_{0};
    std::vector<WorldEvent> journal_;
};

}  // namespace iag
