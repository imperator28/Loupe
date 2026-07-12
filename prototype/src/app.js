import assembly from "./fixture/assembly.json";
import "./styles.css";

const suspiciousUnits = new URLSearchParams(window.location.search).get("fixture") === "suspicious-units";
const state = { workspace: "inspect", activeId: assembly.parts[0].id, selected: new Set() };
const root = document.querySelector("#app");

function part() {
  return assembly.parts.find((value) => value.id === state.activeId);
}

function render() {
  const active = part();
  root.innerHTML = `
    <a class="skip-link" href="#workspace-content">Skip to workspace</a>
    <main id="workspace-content" class="shell" aria-label="Loupe assembly inspector">
      <header class="topbar">
        <div class="brand"><span class="brand-mark">L</span><span>Loupe</span></div>
        <div class="document"><strong>${assembly.name}</strong><span class="unit-chip ${suspiciousUnits ? "review" : ""}">${suspiciousUnits ? "Units need review" : "mm confirmed"}</span></div>
        <button class="command" type="button" aria-label="Search commands">⌘&nbsp;K <span>Commands</span></button>
      </header>
      <section class="workspace-switch" role="tablist" aria-label="Workspace">
        ${tab("inspect", "Inspect")}${tab("export", "Export")}
      </section>
      ${inspect(active, state.workspace !== "inspect")}
      ${exportWorkspace(active, state.workspace !== "export")}
    </main>`;
  bindEvents();
}

function tab(id, label) {
  const selected = state.workspace === id;
  return `<button id="${id}-tab" type="button" role="tab" aria-selected="${selected}" aria-controls="${id}-workspace" tabindex="${selected ? 0 : -1}" data-workspace="${id}">${label}</button>`;
}

function inspect(active, hidden) {
  return `<section id="inspect-workspace" class="inspect-workspace" role="tabpanel" aria-labelledby="inspect-tab" ${hidden ? "hidden" : ""}>
    <aside class="tree-panel"><label for="tree-search">Assembly tree</label><input id="tree-search" name="tree-search" type="search" autocomplete="off" placeholder="Find a component…" />
      <ul>${assembly.parts.map((item) => `<li><button type="button" class="tree-item ${item.id === active.id ? "active" : ""}" data-part="${item.id}"><span>${item.name}</span><small>${item.quantity}×</small></button></li>`).join("")}</ul></aside>
    <section class="master-view" aria-label="Assembly viewer"><div class="grid-plane"></div><div class="assembly-figure"><span class="part-back"></span><span class="part-focus">${active.name}</span><span class="part-front"></span></div><p>Selected: <strong>${active.name}</strong></p></section>
    <aside class="context-panel"><span class="eyebrow">Active component</span><h1>${active.name}</h1><p>${active.kind} · ${active.quantity} occurrence${active.quantity === 1 ? "" : "s"}</p><button type="button">Show properties</button></aside>
    <nav class="inspection-dock" aria-label="Inspection tools">${["Fit", "Isolate", "Section", "Measure", "Ghost", "Capture"].map((name) => `<button type="button" title="${name}" aria-label="${name}"><span aria-hidden="true">${name[0]}</span>${name}</button>`).join("")}</nav>
  </section>`;
}

function exportWorkspace(active, hidden) {
  return `<section id="export-workspace" class="export-workspace" role="tabpanel" aria-labelledby="export-tab" ${hidden ? "hidden" : ""}>
    <aside class="parts-panel"><div><span class="eyebrow">Components</span><h1>Select parts to export</h1></div><ul class="part-list" aria-label="Components">${assembly.parts.map((item) => `<li class="part-row ${item.id === active.id ? "active" : ""}"><button type="button" class="part-select" data-part="${item.id}" aria-pressed="${item.id === active.id}">${item.name}<small>${item.kind} · ${item.quantity}×</small></button><label><input type="checkbox" aria-label="Export ${item.name}" data-check="${item.id}" ${state.selected.has(item.id) ? "checked" : ""}/><span>Export</span></label></li>`).join("")}</ul></aside>
    <section class="master-view export-master" aria-label="Master assembly preview"><span class="eyebrow">Master preview</span><div class="assembly-figure ghosted"><span class="part-back"></span><span class="part-focus">${active.name}</span><span class="part-front"></span></div><p data-testid="master-highlight">${active.name}</p><small>Everything except the active component is ghosted.</small></section>
    <aside class="isolated-view"><span class="eyebrow">Export component</span><div class="isolated-part">${active.name}</div><strong data-testid="isolated-preview">${active.name}</strong><p>Confirm the isolated component before adding it to the export plan.</p><button type="button" ${suspiciousUnits ? "disabled aria-describedby=unit-warning" : ""}>Review export plan</button>${suspiciousUnits ? "<small id=unit-warning>Resolve source units before exporting.</small>" : ""}</aside>
  </section>`;
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
  document.querySelectorAll("[data-part]").forEach((button) => button.addEventListener("click", (event) => { if (event.target.closest("input")) return; state.activeId = button.dataset.part; render(); }));
  document.querySelectorAll("[data-check]").forEach((input) => input.addEventListener("change", () => { input.checked ? state.selected.add(input.dataset.check) : state.selected.delete(input.dataset.check); }));
}

function activateWorkspace(workspace, focus = false) {
  state.workspace = workspace;
  render();
  if (focus) document.querySelector(`#${workspace}-tab`).focus();
}

render();
