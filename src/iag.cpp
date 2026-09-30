#include "iag/iag.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

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

std::string object_id(const ObjectId& id) {
    return quote(id.value);
}

template <typename Map, typename Writer>
std::string map_json(const Map& values, Writer writer) {
    std::string result{"["};
    bool first = true;
    for (const auto& [id, value] : values) {
        if (!first) result += ',';
        first = false;
        result += writer(id, value);
    }
    result += ']';
    return result;
}

std::string state_canonical(const IndustrialState& state) {
    std::ostringstream out;
    out << state.now << '|';
    for (const auto& [id, site] : state.sites) {
        out << "site:" << id.value << ':' << site.name << '|';
    }
    for (const auto& [id, line] : state.lines) {
        out << "line:" << id.value << ':' << line.site.value << ':'
            << line.units_per_hour << ':' << line.available << '|';
    }
    for (const auto& [id, machine] : state.machines) {
        out << "machine:" << id.value << ':' << machine.line.value << ':'
            << static_cast<int>(machine.state) << ':' << machine.vibration_rms
            << ':' << machine.temperature_c << ':' << machine.degradation_index
            << '|';
    }
    for (const auto& [id, product] : state.products) {
        out << "product:" << id.value << ':' << product.primary_line.value
            << ':' << product.alternate_line.value << '|';
    }
    for (const auto& [id, order] : state.work_orders) {
        out << "work:" << id.value << ':' << order.product.value << ':'
            << order.quantity_required << ':' << order.quantity_complete << ':'
            << order.scheduled_quantity << ':' << order.due_minute << ':'
            << static_cast<int>(order.state) << ':' << order.assigned_line.value
            << '|';
    }
    for (const auto& [id, item] : state.inventory) {
        out << "inventory:" << id.value << ':' << item.item.value << ':'
            << item.location.value << ':' << item.on_hand << ':' << item.reserved
            << ':' << item.quarantine << '|';
    }
    for (const auto& [id, task] : state.maintenance) {
        out << "maintenance:" << id.value << ':' << task.machine.value << ':'
            << task.technician.value << ':' << static_cast<int>(task.state) << ':'
            << task.window_start << ':' << task.window_end << '|';
    }
    for (const auto& [id, technician] : state.technicians) {
        out << "technician:" << id.value << ':' << technician.site.value << ':'
            << technician.qualified << ':' << technician.available_from << ':'
            << technician.available_until << '|';
    }
    for (const auto& [id, vehicle] : state.vehicles) {
        out << "vehicle:" << id.value << ':' << vehicle.route.value << ':'
            << vehicle.delay_minutes << '|';
    }
    for (const auto& [id, tariff] : state.tariffs) {
        out << "tariff:" << id.value << ':' << tariff.multiplier << ':'
            << tariff.peak_start << '|';
    }
    for (const auto& [id, commitment] : state.commitments) {
        out << "commitment:" << id.value << ':' << commitment.work_order.value
            << ':' << commitment.due_minute << ':' << commitment.hard_protected
            << '|';
    }
    return out.str();
}

}  // namespace

std::string to_string(Quality quality) {
    switch (quality) {
        case Quality::Valid: return "valid";
        case Quality::Uncertain: return "uncertain";
        case Quality::Stale: return "stale";
        case Quality::Invalid: return "invalid";
        case Quality::Blocked: return "blocked";
    }
    return "invalid";
}

std::string digest(std::string_view canonical) {
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    for (const unsigned char ch : canonical) {
        hash ^= ch;
        hash *= prime;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string to_string(MachineState state) {
    switch (state) {
        case MachineState::Operational: return "operational";
        case MachineState::Degraded: return "degraded";
        case MachineState::Failed: return "failed";
        case MachineState::Maintenance: return "maintenance";
        case MachineState::Offline: return "offline";
    }
    return "offline";
}

std::string to_string(WorkOrderState state) {
    switch (state) {
        case WorkOrderState::Scheduled: return "scheduled";
        case WorkOrderState::InProgress: return "in_progress";
        case WorkOrderState::Complete: return "complete";
        case WorkOrderState::Blocked: return "blocked";
    }
    return "blocked";
}

std::string to_string(MaintenanceState state) {
    switch (state) {
        case MaintenanceState::Planned: return "planned";
        case MaintenanceState::Assigned: return "assigned";
        case MaintenanceState::InProgress: return "in_progress";
        case MaintenanceState::Complete: return "complete";
    }
    return "planned";
}

IndustrialWorld IndustrialWorld::synthetic_enterprise() {
    IndustrialWorld world;
    auto& state = world.state_;
    state.sites.emplace(ObjectId{"SITE-01"}, Site{{"SITE-01"}, "Canopy Plant 01"});
    state.lines.emplace(ObjectId{"LINE-A"},
                        ProductionLine{{"LINE-A"}, {"SITE-01"}, "Line A", 120.0, true});
    state.lines.emplace(ObjectId{"LINE-B"},
                        ProductionLine{{"LINE-B"}, {"SITE-01"}, "Line B", 105.0, true});
    state.machines.emplace(ObjectId{"M-12"},
                           Machine{{"M-12"}, {"SITE-01"}, {"LINE-A"},
                                   MachineState::Operational, 4.2, 52.0, 0.20});
    state.machines.emplace(ObjectId{"M-13"},
                           Machine{{"M-13"}, {"SITE-01"}, {"LINE-B"},
                                   MachineState::Operational, 2.1, 48.0, 0.08});
    state.products.emplace(ObjectId{"SKU-100"},
                           Product{{"SKU-100"}, "SKU-100", {"LINE-A"}, {"LINE-B"}});
    state.products.emplace(ObjectId{"SKU-200"},
                           Product{{"SKU-200"}, "SKU-200", {"LINE-A"}, {"LINE-B"}});
    state.work_orders.emplace(
        ObjectId{"WO-1048"},
        WorkOrder{{"WO-1048"}, {"SKU-100"}, 280.0, 120.0, 160.0, 1320,
                  WorkOrderState::InProgress, {"LINE-A"}});
    state.inventory.emplace(
        ObjectId{"INV-BRG-17"},
        InventoryPosition{{"INV-BRG-17"}, {"BRG-17"}, {"WH-02"}, 1.0, 0.0, 0.0});
    state.warehouses.emplace(ObjectId{"WH-02"},
                             Warehouse{{"WH-02"}, {"SITE-01"}, 10000.0});
    state.technicians.emplace(ObjectId{"T-4"},
                              Technician{{"T-4"}, {"SITE-01"}, true, 300, 540});
    state.vehicles.emplace(ObjectId{"T-7"},
                           Vehicle{{"T-7"}, {"R-7"}, 0});
    state.tariffs.emplace(ObjectId{"SITE-01"},
                          EnergyTariff{{"SITE-01"}, 1.0, 420});
    state.commitments.emplace(
        ObjectId{"CUST-01"},
        CustomerCommitment{{"CUST-01"}, {"WO-1048"}, 1320, true});
    return world;
}

bool IndustrialWorld::apply(IndustrialState& state,
                            const Mutation& mutation,
                            std::string& error) {
    switch (mutation.kind) {
        case MutationKind::SetMachineCondition: {
            const auto it = state.machines.find(mutation.subject);
            if (it == state.machines.end()) {
                error = "MACHINE_NOT_FOUND:" + mutation.subject.value;
                return false;
            }
            auto& machine = it->second;
            machine.vibration_rms = mutation.value;
            machine.temperature_c = mutation.secondary_value;
            machine.degradation_index =
                std::clamp((machine.vibration_rms - 4.0) / 6.0, 0.0, 1.0);
            machine.state = machine.degradation_index >= 1.0
                                ? MachineState::Failed
                                : (machine.degradation_index >= 0.65
                                       ? MachineState::Degraded
                                       : MachineState::Operational);
            state.now = std::max(state.now, mutation.at);
            return true;
        }
        case MutationKind::AssignWorkOrderLine: {
            const auto order_it = state.work_orders.find(mutation.subject);
            const auto line_it = state.lines.find(mutation.destination);
            if (order_it == state.work_orders.end()) {
                error = "WORK_ORDER_NOT_FOUND:" + mutation.subject.value;
                return false;
            }
            if (line_it == state.lines.end() || !line_it->second.available) {
                error = "LINE_UNAVAILABLE:" + mutation.destination.value;
                return false;
            }
            if (mutation.quantity <= 0.0 ||
                mutation.quantity >
                    order_it->second.quantity_required -
                        order_it->second.quantity_complete) {
                error = "INVALID_ASSIGNMENT_QUANTITY";
                return false;
            }
            order_it->second.assigned_line = mutation.destination;
            order_it->second.scheduled_quantity = mutation.quantity;
            return true;
        }
        case MutationKind::CreateMaintenanceTask: {
            if (state.maintenance.contains(mutation.subject)) {
                error = "MAINTENANCE_TASK_EXISTS:" + mutation.subject.value;
                return false;
            }
            if (!state.machines.contains(mutation.destination)) {
                error = "MACHINE_NOT_FOUND:" + mutation.destination.value;
                return false;
            }
            if (!state.technicians.contains(ObjectId{mutation.note})) {
                error = "TECHNICIAN_NOT_FOUND:" + mutation.note;
                return false;
            }
            state.maintenance.emplace(
                mutation.subject,
                MaintenanceTask{mutation.subject, mutation.destination,
                                 ObjectId{mutation.note}, MaintenanceState::Assigned,
                                 mutation.at, mutation.at + 240});
            return true;
        }
        case MutationKind::SetTrafficDelay: {
            const auto it = state.vehicles.find(mutation.subject);
            if (it == state.vehicles.end()) {
                error = "VEHICLE_NOT_FOUND:" + mutation.subject.value;
                return false;
            }
            if (mutation.value < 0.0) {
                error = "NEGATIVE_TRAFFIC_DELAY";
                return false;
            }
            it->second.delay_minutes = static_cast<SimMinute>(mutation.value);
            return true;
        }
        case MutationKind::SetEnergyMultiplier: {
            const auto it = state.tariffs.find(mutation.subject);
            if (it == state.tariffs.end()) {
                error = "TARIFF_NOT_FOUND:" + mutation.subject.value;
                return false;
            }
            if (mutation.value <= 0.0) {
                error = "INVALID_ENERGY_MULTIPLIER";
                return false;
            }
            it->second.multiplier = mutation.value;
            state.now = std::max(state.now, mutation.at);
            return true;
        }
    }
    error = "UNKNOWN_MUTATION";
    return false;
}

bool IndustrialWorld::commit(Revision expected_revision,
                             const std::vector<Mutation>& mutations,
                             std::string& error) {
    if (expected_revision != revision_) {
        error = "STALE_REVISION:expected=" + std::to_string(expected_revision) +
                ",actual=" + std::to_string(revision_);
        return false;
    }
    if (mutations.empty()) {
        error = "EMPTY_TRANSACTION";
        return false;
    }

    IndustrialState candidate = state_;
    for (const auto& mutation : mutations) {
        if (!apply(candidate, mutation, error)) {
            return false;
        }
    }

    state_ = std::move(candidate);
    ++revision_;
    for (const auto& mutation : mutations) {
        journal_.push_back(WorldEvent{revision_, std::to_string(static_cast<int>(mutation.kind)),
                                      mutation.subject, mutation.note});
    }
    constexpr std::size_t journal_limit = 256;
    if (journal_.size() > journal_limit) {
        journal_.erase(journal_.begin(),
                       journal_.begin() +
                           static_cast<std::ptrdiff_t>(journal_.size() - journal_limit));
    }
    return true;
}

std::string IndustrialWorld::state_digest() const {
    return digest(state_canonical(state_));
}

std::string IndustrialWorld::to_json() const {
    std::string result{"{\n  \"revision\":"};
    result += std::to_string(revision_);
    result += ",\n  \"state_digest\":" + quote(state_digest());
    result += ",\n  \"now_minute\":" + std::to_string(state_.now);
    result += ",\n  \"sites\":" +
              map_json(state_.sites, [](const auto&, const Site& site) {
                  return "{\"id\":" + object_id(site.id) + ",\"name\":" +
                         quote(site.name) + "}";
              });
    result += ",\n  \"lines\":" +
              map_json(state_.lines, [](const auto&, const ProductionLine& line) {
                  return "{\"id\":" + object_id(line.id) + ",\"site\":" +
                         object_id(line.site) + ",\"available\":" +
                         (line.available ? "true" : "false") + "}";
              });
    result += ",\n  \"machines\":" +
              map_json(state_.machines, [](const auto&, const Machine& machine) {
                  return "{\"id\":" + object_id(machine.id) +
                         ",\"line\":" + object_id(machine.line) +
                         ",\"state\":" + quote(to_string(machine.state)) +
                         ",\"vibration_rms\":" + number(machine.vibration_rms) +
                         ",\"temperature_c\":" + number(machine.temperature_c) +
                         ",\"degradation_index\":" +
                         number(machine.degradation_index) + "}";
              });
    result += ",\n  \"work_orders\":" +
              map_json(state_.work_orders, [](const auto&, const WorkOrder& order) {
                  return "{\"id\":" + object_id(order.id) +
                         ",\"product\":" + object_id(order.product) +
                         ",\"quantity_required\":" + number(order.quantity_required) +
                         ",\"quantity_complete\":" + number(order.quantity_complete) +
                         ",\"scheduled_quantity\":" +
                         number(order.scheduled_quantity) +
                         ",\"assigned_line\":" + object_id(order.assigned_line) +
                         ",\"state\":" + quote(to_string(order.state)) + "}";
              });
    result += ",\n  \"inventory\":" +
              map_json(state_.inventory, [](const auto&, const InventoryPosition& item) {
                  return "{\"id\":" + object_id(item.id) +
                         ",\"available\":" + number(item.available()) + "}";
              });
    result += ",\n  \"maintenance\":" +
              map_json(state_.maintenance,
                       [](const auto&, const MaintenanceTask& task) {
                           return "{\"id\":" + object_id(task.id) +
                                  ",\"machine\":" + object_id(task.machine) +
                                  ",\"technician\":" + object_id(task.technician) +
                                  ",\"state\":" +
                                  quote(to_string(task.state)) + "}";
                       });
    result += ",\n  \"vehicles\":" +
              map_json(state_.vehicles, [](const auto&, const Vehicle& vehicle) {
                  return "{\"id\":" + object_id(vehicle.id) +
                         ",\"route\":" + object_id(vehicle.route) +
                         ",\"delay_minutes\":" +
                         std::to_string(vehicle.delay_minutes) + "}";
              });
    result += ",\n  \"tariffs\":" +
              map_json(state_.tariffs, [](const auto&, const EnergyTariff& tariff) {
                  return "{\"site\":" + object_id(tariff.site) +
                         ",\"multiplier\":" + number(tariff.multiplier) + "}";
              });
    result += "\n}\n";
    return result;
}

std::string IndustrialWorld::events_json() const {
    std::string result;
    for (const auto& event : journal_) {
        result += "{\"revision\":" + std::to_string(event.revision) +
                  ",\"kind\":" + quote(event.kind) +
                  ",\"subject\":" + object_id(event.subject) +
                  ",\"detail\":" + quote(event.detail) + "}\n";
    }
    return result;
}

std::string to_string(EvidenceKind kind) {
    switch (kind) {
        case EvidenceKind::MachineCondition: return "machine_condition";
        case EvidenceKind::TrafficDelay: return "traffic_delay";
        case EvidenceKind::EnergyTariff: return "energy_tariff";
    }
    return "unknown";
}

namespace {

std::string trim(std::string value) {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string after_colon(const std::string& line) {
    const auto position = line.find(':');
    return position == std::string::npos ? std::string{} :
           trim(line.substr(position + 1));
}

double parse_number(const std::string& value, bool& ok) {
    try {
        std::size_t used = 0;
        const double number = std::stod(value, &used);
        ok = used == value.size();
        return number;
    } catch (...) {
        ok = false;
        return 0.0;
    }
}

SimMinute parse_time(const std::string& value, bool& ok) {
    const auto first = value.find(':');
    const auto second = value.find(':', first + 1);
    if (first == std::string::npos || second == std::string::npos) {
        ok = false;
        return 0;
    }
    bool hour_ok = false;
    bool minute_ok = false;
    bool second_ok = false;
    const auto hour = parse_number(value.substr(0, first), hour_ok);
    const auto minute =
        parse_number(value.substr(first + 1, second - first - 1), minute_ok);
    const auto seconds = parse_number(value.substr(second + 1), second_ok);
    ok = hour_ok && minute_ok && second_ok;
    return ok ? static_cast<SimMinute>(hour * 60.0 + minute + seconds / 60.0)
              : 0;
}

}  // namespace

ScenarioDefinition load_scenario_file(std::string_view path, std::string& error) {
    std::ifstream input{std::string(path)};
    if (!input) {
        error = "SCENARIO_NOT_FOUND:" + std::string(path);
        return {};
    }

    ScenarioDefinition scenario;
    std::string line;
    ScenarioEvent pending;
    bool have_pending = false;
    auto flush = [&]() {
        if (have_pending) {
            scenario.events.push_back(pending);
            pending = ScenarioEvent{};
            have_pending = false;
        }
    };

    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        if (line.rfind("scenario:", 0) == 0) {
            scenario.name = after_colon(line);
        } else if (line.rfind("seed:", 0) == 0) {
            bool ok = false;
            scenario.seed = static_cast<std::uint64_t>(parse_number(after_colon(line), ok));
            if (!ok) {
                error = "INVALID_SCENARIO_SEED";
                return {};
            }
        } else if (line.rfind("- at:", 0) == 0) {
            flush();
            pending = ScenarioEvent{};
            have_pending = true;
            bool ok = false;
            pending.at = parse_time(trim(line.substr(line.find("at:") + 3)), ok);
            if (!ok) {
                error = "INVALID_SCENARIO_TIME";
                return {};
            }
        } else if (line.find("type: machine_condition") != std::string::npos) {
            if (!have_pending) {
                pending = ScenarioEvent{};
                have_pending = true;
            }
            pending.kind = EvidenceKind::MachineCondition;
        } else if (line.find("type: traffic_delay") != std::string::npos) {
            if (!have_pending) {
                pending = ScenarioEvent{};
                have_pending = true;
            }
            pending.kind = EvidenceKind::TrafficDelay;
        } else if (line.find("type: energy_tariff") != std::string::npos) {
            if (!have_pending) {
                pending = ScenarioEvent{};
                have_pending = true;
            }
            pending.kind = EvidenceKind::EnergyTariff;
        } else if (line.rfind("at:", 0) == 0 && have_pending) {
            bool ok = false;
            pending.at = parse_time(after_colon(line), ok);
            if (!ok) {
                error = "INVALID_SCENARIO_TIME";
                return {};
            }
        } else if (line.rfind("machine:", 0) == 0 && have_pending) {
            pending.subject = ObjectId{after_colon(line)};
        } else if (line.rfind("route:", 0) == 0 && have_pending) {
            pending.subject = ObjectId{after_colon(line)};
        } else if (line.rfind("site:", 0) == 0 && have_pending) {
            pending.subject = ObjectId{after_colon(line)};
        } else if (line.rfind("vibration_rms:", 0) == 0 && have_pending) {
            bool ok = false;
            pending.value = parse_number(after_colon(line), ok);
            if (!ok) {
                error = "INVALID_VIBRATION_VALUE";
                return {};
            }
        } else if (line.rfind("delay_minutes:", 0) == 0 && have_pending) {
            bool ok = false;
            pending.value = parse_number(after_colon(line), ok);
            if (!ok) {
                error = "INVALID_DELAY_VALUE";
                return {};
            }
        } else if (line.rfind("multiplier:", 0) == 0 && have_pending) {
            bool ok = false;
            pending.value = parse_number(after_colon(line), ok);
            if (!ok) {
                error = "INVALID_MULTIPLIER_VALUE";
                return {};
            }
        }
    }
    flush();
    if (scenario.name.empty() || scenario.events.empty()) {
        error = "SCENARIO_MISSING_NAME_OR_EVENTS";
        return {};
    }
    return scenario;
}

bool run_scenario_events(IndustrialWorld& world,
                         const ScenarioDefinition& scenario,
                         std::vector<EvidenceItem>& evidence,
                         std::vector<Finding>& findings,
                         std::string& error) {
    std::uint64_t sequence = 0;
    for (const auto& event : scenario.events) {
        ++sequence;
        EvidenceItem item;
        item.id = "E-" + std::to_string(sequence);
        item.source = event.kind == EvidenceKind::MachineCondition
                          ? "sim.telemetry"
                          : event.kind == EvidenceKind::TrafficDelay
                                ? "sim.traffic"
                                : "sim.energy";
        item.kind = event.kind;
        item.subject = event.subject;
        item.observed_at = event.at;
        item.sequence = sequence;
        item.value = event.value;
        item.quality = Quality::Valid;
        item.raw_hash = digest(item.source + "|" + item.subject.value + "|" +
                               std::to_string(item.observed_at) + "|" +
                               std::to_string(item.value));

        Mutation mutation;
        if (event.kind == EvidenceKind::MachineCondition) {
            mutation = Mutation{MutationKind::SetMachineCondition, event.subject, {},
                                event.value, 50.0 + event.value * 2.0, 0.0,
                                event.at, item.id};
        } else if (event.kind == EvidenceKind::TrafficDelay) {
            mutation = Mutation{MutationKind::SetTrafficDelay, {"T-7"}, {},
                                event.value, 0.0, 0.0, event.at, item.id};
        } else {
            mutation = Mutation{MutationKind::SetEnergyMultiplier, event.subject, {},
                                event.value, 0.0, 0.0, event.at, item.id};
        }

        const auto revision = world.revision();
        if (!world.commit(revision, {mutation}, error)) {
            return false;
        }
        evidence.push_back(std::move(item));
    }

    const auto machine_it = world.state().machines.find(ObjectId{"M-12"});
    if (machine_it != world.state().machines.end()) {
        std::vector<std::string> supporting;
        double first_value = 0.0;
        SimMinute first_time = 0;
        double latest_value = 0.0;
        SimMinute latest_time = 0;
        bool first = true;
        for (const auto& item : evidence) {
            if (item.kind == EvidenceKind::MachineCondition &&
                item.subject == ObjectId{"M-12"}) {
                if (first) {
                    first_value = item.value;
                    first_time = item.observed_at;
                    first = false;
                }
                latest_value = item.value;
                latest_time = item.observed_at;
                supporting.push_back(item.id);
            }
        }
        if (!supporting.empty()) {
            const double slope = latest_time == first_time
                                     ? 0.0
                                     : (latest_value - first_value) /
                                           static_cast<double>(latest_time - first_time);
            findings.push_back(Finding{"F-HEALTH-M12", "degradation_trend",
                                       {"M-12"}, slope, 0.98, supporting,
                                       "Vibration is rising; the finding is derived from admitted telemetry."});
            findings.push_back(Finding{"F-FORECAST-M12", "failure_forecast",
                                       {"M-12"}, machine_it->second.degradation_index,
                                       0.91, supporting,
                                       "Forecast is predicted state, not observed failure truth."});
        }
    }
    return true;
}

std::string evidence_jsonl(const std::vector<EvidenceItem>& evidence) {
    std::string result;
    for (const auto& item : evidence) {
        result += "{\"id\":" + quote(item.id) + ",\"source\":" +
                  quote(item.source) + ",\"kind\":" +
                  quote(to_string(item.kind)) + ",\"subject\":" +
                  object_id(item.subject) + ",\"observed_at\":" +
                  std::to_string(item.observed_at) + ",\"sequence\":" +
                  std::to_string(item.sequence) + ",\"value\":" +
                  number(item.value) + ",\"quality\":" +
                  quote(to_string(item.quality)) + ",\"raw_hash\":" +
                  quote(item.raw_hash) + "}\n";
    }
    return result;
}

std::string findings_jsonl(const std::vector<Finding>& findings) {
    std::string result;
    for (const auto& finding : findings) {
        result += "{\"id\":" + quote(finding.id) + ",\"kind\":" +
                  quote(finding.kind) + ",\"subject\":" +
                  object_id(finding.subject) + ",\"value\":" +
                  number(finding.value) + ",\"confidence\":" +
                  number(finding.confidence) + ",\"explanation\":" +
                  quote(finding.explanation) + ",\"evidence_ids\":[";
        for (std::size_t index = 0; index < finding.evidence_ids.size(); ++index) {
            if (index > 0) result += ',';
            result += quote(finding.evidence_ids[index]);
        }
        result += "]}\n";
    }
    return result;
}

}  // namespace iag
