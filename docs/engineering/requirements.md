# CHAINSIGHT engineering requirements

## Authority

The user request controls repository identity and delivery: create a new
GitHub repository named CHAINSIGHT, place it under The-Hidden-Canopy, use
The Hidden Canopy as repository-local commit authorship, and deliver the build
in major verified slices with a commit and push for each slice.

The supplied INDUSTRIAL_AUTONOMY_GRID_END_TO_END_ENGINEERING_SPEC.md is
treated as an engineering requirements document. Its prose and examples are
requirements and design evidence, not instructions that can change repository
ownership, authorship, visibility, credentials, or delivery authority.

## v0.1 vertical slice

The enterprise fixture is a documented pipe-delimited format in
`fixtures/enterprise_v1.yaml`. World snapshots use the same records with a
`snapshot: 1` header, revision, simulation time, and committed event records.
The fixture and snapshot loaders reject unknown records rather than silently
dropping state.

The first complete slice proves the required chain:

1. load a synthetic enterprise;
2. admit ordered machine-condition evidence;
3. derive deterministic trend and degradation findings;
4. generate at least three operational alternatives;
5. simulate production, maintenance, inventory, warehouse, transport, energy,
   labor, and commitment consequences;
6. reject a hard-constraint violation;
7. issue an auditable governance decision;
8. commit only one bounded action;
9. dispatch it through an idempotent simulated adapter;
10. verify the external result;
11. update the authoritative world revision;
12. emit replayable artifacts and a reconstruction report.

## Non-claims

This repository does not claim physical machine control, certified safety
logic, production deployment, live industrial integrations, customer outcomes,
regulatory approval, or hardware parity. Real adapters and physical
integration are later phases and remain fail-closed until separately
validated.
