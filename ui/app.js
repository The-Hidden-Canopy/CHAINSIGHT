const artifactRoot = "../artifacts/bearing_failure_compound/";
const byId = (id) => document.getElementById(id);

function text(value, fallback = "Unavailable") {
  return value === undefined || value === null || value === "" ? fallback : String(value);
}

async function loadJson(name) {
  const response = await fetch(artifactRoot + name, { cache: "no-store" });
  if (!response.ok) throw new Error(name + " returned " + response.status);
  return response.json();
}

async function loadJsonl(name) {
  const response = await fetch(artifactRoot + name, { cache: "no-store" });
  if (!response.ok) throw new Error(name + " returned " + response.status);
  const body = await response.text();
  return body.trim() ? body.trim().split(/\r?\n/).map((line) => JSON.parse(line)) : [];
}

function renderEvidence(items) {
  byId("evidence-count").textContent = items.length;
  byId("evidence-quality").textContent = items.every((item) => item.quality === "valid") ? "VALID INPUTS" : "QUALITY MIXED";
  const rows = items.map((item) => "<tr><td>" + text(item.sequence) + "</td><td>" + text(item.source) + "</td><td>" + text(item.kind) + "</td><td>" + text(item.subject) + "</td><td>" + text(item.value) + "</td><td><span class=\"quality\">" + text(item.quality) + "</span></td></tr>").join("");
  byId("evidence-list").innerHTML = "<table><thead><tr><th>Sequence</th><th>Source</th><th>Kind</th><th>Subject</th><th>Value</th><th>Quality</th></tr></thead><tbody>" + rows + "</tbody></table>";
}

function renderFindings(items) {
  byId("finding-list").innerHTML = items.map((item) => "<div class=\"finding\"><strong>" + text(item.kind) + "</strong><span>" + text(item.subject) + "</span><span>value " + text(item.value) + "</span><span>confidence " + text(item.confidence) + "</span><p>" + text(item.explanation) + "</p></div>").join("");
}

function renderCandidates(data) {
  const candidates = data.candidates || [];
  byId("candidate-count").textContent = candidates.length;
  byId("selected-candidate").textContent = "SELECTED " + text(data.selected_candidate);
  byId("candidate-list").innerHTML = candidates.map((candidate) => {
    const simulation = candidate.simulation || {};
    const chosen = candidate.id === data.selected_candidate;
    const violation = (candidate.violations || []).join("; ");
    return "<article class=\"candidate " + (chosen ? "chosen " : "") + (candidate.hard_constraints_ok ? "" : "rejected") + "\"><div class=\"candidate-title\"><h3>" + text(candidate.id) + " · " + text(candidate.strategy) + "</h3><span class=\"badge\">" + (candidate.hard_constraints_ok ? "FEASIBLE" : "REJECTED") + "</span></div><div class=\"candidate-metrics\"><span>On-time <b>" + text(simulation.on_time_probability) + "</b></span><span>Lateness <b>" + text(simulation.expected_lateness_minutes) + "m</b></span><span>Energy <b>" + text(simulation.energy_cost) + "</b></span></div><p>" + text(simulation.consequence_summary) + (violation ? " <strong>Constraint:</strong> " + violation : "") + "</p></article>";
  }).join("");
}

function renderDecision(decision) {
  const request = decision.request || {};
  byId("decision-card").classList.remove("unavailable");
  byId("decision-card").innerHTML = "<strong class=\"decision-" + String(decision.action).toLowerCase() + "\">" + text(decision.action) + "</strong><dl><dt>Policy epoch</dt><dd>" + text(decision.policy_epoch) + "</dd><dt>Action</dt><dd>" + text(request.action) + "</dd><dt>Quantity</dt><dd>" + text(request.quantity) + "</dd><dt>Hash</dt><dd class=\"mono\">" + text(decision.decision_hash) + "</dd></dl><ul>" + (decision.reasons || []).map((reason) => "<li>" + reason + "</li>").join("") + "</ul>";
}

function renderExecution(receipt) {
  byId("execution-status").textContent = text(receipt.status);
  byId("execution-detail").textContent = text(receipt.detail);
  byId("execution-card").classList.remove("unavailable");
  byId("execution-card").innerHTML = "<strong class=\"" + (receipt.verified ? "success" : "warning") + "\">" + (receipt.verified ? "VERIFIED" : "NOT VERIFIED") + "</strong><dl><dt>Dispatched</dt><dd>" + (receipt.dispatched ? "yes" : "no") + "</dd><dt>Revision</dt><dd>" + text(receipt.before_revision) + " → " + text(receipt.after_revision) + "</dd><dt>Idempotency</dt><dd class=\"mono\">" + text(receipt.idempotency_key) + "</dd></dl><p>" + text(receipt.detail) + "</p>";
}

async function loadWorkspace() {
  const results = await Promise.all([loadJson("world_after.json"), loadJsonl("evidence.jsonl"), loadJsonl("findings.jsonl"), loadJson("candidates.json"), loadJson("decision.json"), loadJson("execution_receipt.json")]);
  const world = results[0];
  byId("world-revision").textContent = text(world.revision);
  byId("world-digest").textContent = "digest " + text(world.state_digest);
  renderEvidence(results[1]);
  renderFindings(results[2]);
  renderCandidates(results[3]);
  renderDecision(results[4]);
  renderExecution(results[5]);
  byId("run-status").textContent = "SCENARIO LOADED";
  byId("run-status").className = "status status-ok";
}

loadWorkspace().catch((error) => {
  byId("run-status").textContent = "DATA UNAVAILABLE";
  byId("run-status").className = "status status-error";
  console.warn("CHAINSIGHT evidence workspace unavailable:", error.message);
});

