# Productization slice

The operator workspace in ui/ is deliberately read-only. It reads the
scenario runner's JSON and JSONL artifacts and displays:

- current world revision and digest;
- admitted evidence with source, sequence, value, and quality;
- derived findings with confidence and lineage identifiers;
- candidate simulations and explicit hard-constraint rejection;
- governance policy epoch, action, reasons, and decision hash;
- bounded execution status, revision transition, verification, and idempotency.

If an artifact is missing or the local server cannot fetch it, the UI shows
Unavailable or DATA UNAVAILABLE. It does not invent a metric, receipt, winner,
or backend action.

From the repository root, serve the UI after running the scenario:

~~~powershell
python -m http.server 8080
~~~

Then open http://localhost:8080/ui/. A future native shell can reuse the same
artifact boundary without making the UI authoritative.

