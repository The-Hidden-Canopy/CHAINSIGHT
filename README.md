# CHAINSIGHT

CHAINSIGHT is The Hidden Canopy's standalone industrial operations world for
simulation, planning, governance, bounded execution, verification, and replay.

The product is built around one revisioned IndustrialWorld:

~~~text
observe
  -> validate and admit evidence
  -> update world and belief
  -> forecast
  -> generate bounded candidates
  -> simulate consequences
  -> filter hard constraints
  -> govern
  -> commit one bounded step
  -> execute through an adapter
  -> verify
  -> reconcile and replan
~~~

CHAINSIGHT is not an LLM controlling PLCs, a replacement for ERP/MES/WMS/TMS
systems, a safety controller, or a claim of physical autonomy. The v0.1
implementation is deterministic, simulation-first, read-only toward external
systems, and explicit about unknown, stale, contested, and predicted state.

## Build

The repository is dependency-free and targets C++23.

~~~powershell
cmake -S . -B build -G "Visual Studio 18 2026"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
~~~

## Canonical scenario

After building:

~~~powershell
build\Debug\iag_scenario_runner.exe scenarios\bearing_failure_compound.yaml
~~~

The runner writes a replayable evidence, planning, governance, execution, and
world-transition artifact set under artifacts\bearing_failure_compound. It
also runs the scenario a second time and writes replay_verification.json only
when every deterministic artifact matches. Use --no-replay-check only when
debugging a single run.

## Engineering boundaries

- One authoritative revisioned world; no agent-owned mutable reality.
- Evidence is not truth and prediction is not execution.
- Plans are simulated on snapshots and execution commits one bounded step.
- Hard constraints are explicit and cannot be traded away by scoring.
- External writes are represented by idempotent simulated adapters in v0.1.
- Evidence admission rejects invalid quality, stale, replayed, contradictory,
  and hash-mismatched records before world mutation.
- World snapshots and simulated receipt stores can be reloaded after restart.
- Hardware, physical actuation, cloud collection, and production deployment
  remain later, explicitly gated work.

See the engineering requirements in docs/engineering/requirements.md and the
user-supplied end-to-end specification for the full roadmap.
