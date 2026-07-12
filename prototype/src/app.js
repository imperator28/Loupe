import {
  AlertTriangle,
  Camera,
  CheckCircle2,
  createIcons,
  EyeOff,
  Focus,
  Maximize2,
  Ruler,
  Scissors
} from "lucide";
import assembly from "./fixture/assembly.json";
import "./styles.css";

const suspiciousUnits = new URLSearchParams(window.location.search).get("fixture") === "suspicious-units";
const importedBoundsMm = [238, 148, 62];
const iconSet = { AlertTriangle, Camera, CheckCircle2, EyeOff, Focus, Maximize2, Ruler, Scissors };
const tools = [
  ["Fit", "maximize-2"],
  ["Isolate", "focus"],
  ["Section", "scissors"],
  ["Measure", "ruler"],
  ["Ghost", "eye-off"],
  ["Capture", "camera"]
];
const state = { workspace: "inspect", activeId: null, selected: new Set(), unitReviewOpen: false, effectiveUnit: suspiciousUnits ? null : "mm", proposedUnit: suspiciousUnits ? "in" : "mm" };
const root = document.querySelector("#app");

function part() {
  return assembly.parts.find((value) => value.id === state.activeId) ?? null;
}

function render() {
  const active = part();
  root.innerHTML = `
    <a class="skip-link" href="#workspace-content">Skip to workspace</a>
    <main id="workspace-content" class="shell" aria-label="Loupe assembly inspector">
      <header class="topbar">
        <div class="brand"><span class="brand-mark" aria-hidden="true">L</span><span>Loupe</span></div>
        <div class="document"><strong>${assembly.name}</strong></div>
        <section class="workspace-switch" role="tablist" aria-label="Workspace">
          ${tab("inspect", "Inspect")}${tab("export", "Export")}
        </section>
        ${unitControl()}
      </header>
      ${inspect(active, state.workspace !== "inspect")}
      ${exportWorkspace(active, state.workspace !== "export")}
    </main>${state.unitReviewOpen ? unitReviewDialog() : ""}`;
  createIcons({ icons: iconSet });
  bindEvents();
}

function tab(id, label) {
  const selected = state.workspace === id;
  return `<button id="${id}-tab" type="button" role="tab" aria-selected="${selected}" aria-controls="${id}-workspace" tabindex="${selected ? 0 : -1}" data-workspace="${id}">${label}</button>`;
}

function unitControl() {
  const status = unitStatus();
  const icon = state.effectiveUnit ? "check-circle-2" : "alert-triangle";
  return `<button class="unit-summary ${state.effectiveUnit ? "" : "review"}" type="button" data-open-unit-review aria-label="Review source units: ${boundsFor(state.effectiveUnit ?? "mm")}; ${status}" aria-haspopup="dialog" title="Review source units and conversion">
    <i data-lucide="${icon}" aria-hidden="true"></i>
    <span><strong>${boundsFor(state.effectiveUnit ?? "mm")}</strong><small>${status}</small></span>
  </button>`;
}

function inspect(active, hidden) {
  return `<section id="inspect-workspace" class="inspect-workspace" role="tabpanel" aria-labelledby="inspect-tab" ${hidden ? "hidden" : ""}>
    <aside class="tree-panel"><label for="tree-search">Assembly tree</label><input id="tree-search" name="tree-search" type="search" autocomplete="off" placeholder="Find a component…" />
      <ul>${assembly.parts.map((item) => `<li><button type="button" class="tree-item ${item.id === active?.id ? "active" : ""}" data-part="${item.id}"><span>${item.name}</span><small>${item.quantity}×</small></button></li>`).join("")}</ul></aside>
    <section class="master-view" aria-label="Assembly viewer"><div class="grid-plane"></div>${assemblyFigure(active)}<p class="view-state">${active ? `Selected: <strong>${active.name}</strong>` : "Full assembly"}</p>${inspectionDock()}</section>
    <aside class="context-panel">${inspector(active)}</aside>
  </section>`;
}

function assemblyFigure(active, ghosted = false) {
  return `<div class="assembly-figure ${ghosted && active ? "ghosted" : ""}"><span class="part-back"></span><span class="part-focus ${active ? "selected" : ""}">${active?.name ?? ""}</span><span class="part-front"></span></div>`;
}

function inspector(active) {
  if (!active) {
    return `<span class="eyebrow">Document properties</span><h1>${assembly.name}</h1><dl class="properties"><div><dt>Components</dt><dd>${assembly.parts.length} definitions</dd></div><div><dt>Occurrences</dt><dd>${assembly.parts.reduce((total, item) => total + item.quantity, 0)}</dd></div><div><dt>Overall bounds</dt><dd>${boundsFor(state.effectiveUnit ?? "mm")}</dd></div><div><dt>Effective unit</dt><dd>${unitStatus()}</dd></div></dl>`;
  }
  return `<span class="eyebrow">Component properties</span><h1>${active.name}</h1><dl class="properties"><div><dt>Type</dt><dd>${active.kind}</dd></div><div><dt>Occurrences</dt><dd>${active.quantity}</dd></div><div><dt>Export scope</dt><dd>Occurrence</dd></div><div><dt>Source unit</dt><dd>${unitStatus()}</dd></div></dl>`;
}

function inspectionDock() {
  return `<nav class="inspection-dock" aria-label="Inspection tools">${tools.map(([name, icon]) => `<button type="button" title="${name}" aria-label="${name}"><i data-lucide="${icon}" aria-hidden="true"></i><span>${name}</span></button>`).join("")}</nav>`;
}

function exportWorkspace(active, hidden) {
  const activeName = active?.name ?? "Full assembly";
  return `<section id="export-workspace" class="export-workspace" role="tabpanel" aria-labelledby="export-tab" ${hidden ? "hidden" : ""}>
    <aside class="parts-panel"><div><span class="eyebrow">Components</span><h1>Select parts to export</h1></div><ul class="part-list" aria-label="Components">${assembly.parts.map((item) => `<li class="part-row ${item.id === active?.id ? "active" : ""}"><button type="button" class="part-select" data-part="${item.id}" aria-pressed="${item.id === active?.id}">${item.name}<small>${item.kind} · ${item.quantity}×</small></button><label><input type="checkbox" aria-label="Export ${item.name}" data-check="${item.id}" ${state.selected.has(item.id) ? "checked" : ""}/><span>Export</span></label></li>`).join("")}</ul></aside>
    <section class="master-view export-master" aria-label="Master assembly preview"><span class="eyebrow">Master preview</span>${assemblyFigure(active, true)}<p data-testid="master-highlight">${activeName}</p><small>${active ? "Everything except the active component is ghosted." : "Select a component to confirm it in context."}</small></section>
    <aside class="isolated-view"><span class="eyebrow">Export component</span><div class="isolated-part">${active?.name ?? "Select a component"}</div><strong data-testid="isolated-preview">${active?.name ?? "No component selected"}</strong><p>${active ? "Confirm the isolated component before adding it to the export plan." : "Choose a component from the list to review its isolated export preview."}</p><button type="button" ${state.effectiveUnit ? "" : "disabled aria-describedby=unit-warning"}>Review export plan</button>${state.effectiveUnit ? "" : "<small id=unit-warning>Resolve source units before exporting.</small>"}</aside>
  </section>`;
}

function unitReviewDialog() {
  return `<div class="dialog-scrim"><section class="unit-review-dialog" role="dialog" aria-modal="true" aria-labelledby="unit-review-title"><div><span class="eyebrow">Import interpretation</span><h1 id="unit-review-title">Source unit review</h1></div><button class="dialog-close" type="button" data-close-unit-review aria-label="Close source unit review">×</button><label class="unit-select-label" for="unit-proposal">Interpret source dimensions as<select id="unit-proposal" data-unit-proposal aria-label="Interpret source dimensions as"><option value="mm" ${state.proposedUnit === "mm" ? "selected" : ""}>Millimeters (mm)</option><option value="in" ${state.proposedUnit === "in" ? "selected" : ""}>Inches (in)</option></select></label><dl class="properties"><div><dt>Preview bounds</dt><dd>${boundsFor(state.proposedUnit)}</dd></div><div><dt>Current status</dt><dd>${unitStatus()}</dd></div></dl><p>${state.effectiveUnit ? "Applying a different interpretation changes only this reviewed session; it never mutates the source STEP file." : "Export remains blocked until you apply a reviewed interpretation."}</p><div class="dialog-actions"><button type="button" data-apply-unit-review>Apply unit interpretation</button><button type="button" data-cancel-unit-review>Cancel unit review</button></div></section></div>`;
}

function bindEvents() {
  document.querySelectorAll("[data-workspace]").forEach((button) => {
    button.addEventListener("click", () => activateWorkspace(button.dataset.workspace));
    button.addEventListener("keydown", (event) => {
      const tabs = [...document.querySelectorAll("[role=tab]")];
      const index = tabs.indexOf(button);
      const targetIndex = event.key === "Home" ? 0 : event.key === "End" ? tabs.length - 1 : event.key === "ArrowRight" ? (index + 1) % tabs.length : event.key === "ArrowLeft" ? (index - 1 + tabs.length) % tabs.length : null;
      if (targetIndex === null) return;
      event.preventDefault();
      activateWorkspace(tabs[targetIndex].dataset.workspace, true);
    });
  });
  document.querySelectorAll("[data-part]").forEach((button) => button.addEventListener("click", () => { state.activeId = button.dataset.part; render(); }));
  document.querySelectorAll("[data-check]").forEach((input) => input.addEventListener("change", () => { input.checked ? state.selected.add(input.dataset.check) : state.selected.delete(input.dataset.check); }));
  document.querySelector("[data-open-unit-review]")?.addEventListener("click", () => { state.unitReviewOpen = true; render(); });
  document.querySelector("[data-close-unit-review]")?.addEventListener("click", cancelUnitReview);
  document.querySelector("[data-cancel-unit-review]")?.addEventListener("click", cancelUnitReview);
  document.querySelector("[data-apply-unit-review]")?.addEventListener("click", applyUnitReview);
  document.querySelector("[data-unit-proposal]")?.addEventListener("change", (event) => { state.proposedUnit = event.target.value; render(); });
  const dialog = document.querySelector(".unit-review-dialog");
  if (dialog) {
    document.querySelector("main").inert = true;
    document.querySelector(".skip-link").inert = true;
    dialog.addEventListener("keydown", trapDialogFocus);
    document.querySelector("[data-close-unit-review]").focus();
  }
}

function activateWorkspace(workspace, focus = false) {
  state.workspace = workspace;
  render();
  if (focus) document.querySelector(`#${workspace}-tab`).focus();
}

function cancelUnitReview() {
  state.unitReviewOpen = false;
  state.proposedUnit = state.effectiveUnit ?? "in";
  render();
  document.querySelector("[data-open-unit-review]").focus();
}

function applyUnitReview() {
  state.effectiveUnit = state.proposedUnit;
  state.unitReviewOpen = false;
  render();
  document.querySelector("[data-open-unit-review]").focus();
}

function trapDialogFocus(event) {
  if (event.key === "Escape") {
    event.preventDefault();
    event.stopPropagation();
    cancelUnitReview();
    return;
  }
  if (event.key !== "Tab") return;
  const focusable = [...event.currentTarget.querySelectorAll("button, select")];
  const first = focusable[0];
  const last = focusable.at(-1);
  if (event.shiftKey && document.activeElement === first) {
    event.preventDefault();
    last.focus();
  } else if (!event.shiftKey && document.activeElement === last) {
    event.preventDefault();
    first.focus();
  }
}

function unitStatus() {
  return state.effectiveUnit ? `${state.effectiveUnit} confirmed` : "Units need review";
}

function boundsFor(unit) {
  const divisor = unit === "in" ? 25.4 : 1;
  const values = importedBoundsMm.map((value) => (value / divisor).toFixed(unit === "in" ? 2 : 0));
  return `${values.join(" × ")} ${unit}`;
}

document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && !state.unitReviewOpen && state.activeId) {
    state.activeId = null;
    render();
  }
});

render();
