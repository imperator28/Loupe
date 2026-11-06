// Loupe landing v2 (prototype).
// One pinned "theatre": the hero laptop zooms until its screen becomes the stage, then scroll
// drives a single state object. A timeline tweens numbers in that object; apply() turns the
// numbers into the 3D model, the app chrome and the page-scale callouts, so scrubbing backwards
// always lands in a consistent state.
import * as THREE from 'three';

const gsap = window.gsap;
const ScrollTrigger = window.ScrollTrigger;
const reduceMotion = matchMedia('(prefers-reduced-motion: reduce)').matches;
const motion = !!(gsap && ScrollTrigger) && !reduceMotion;

const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];
const clamp = (v, a = 0, b = 1) => Math.min(b, Math.max(a, v));
const lerp = (a, b, t) => a + (b - a) * t;
const seg = (v, a, b) => clamp((v - a) / (b - a)); // 0..1 progress of v through [a, b]

const theater = $('#theater');
const rig = $('#rig');
const app = $('#app');
const screenEl = $('#screen');
const viewportEl = $('#viewport');
const overlay = $('#overlay');

// ---------------------------------------------------------------------------------------------
// The model: a small linear stage built in code, in millimetres, Y up. Stand-in for a real
// assembly exported from Loupe; every number shown on the page is computed from this geometry.
// ---------------------------------------------------------------------------------------------
const DENSITY = { 'Aluminium 6061': 2.70, 'Steel': 7.85, 'PA 66': 1.14, 'Bronze': 8.80 };
const MAT_COLOR = { 'Aluminium 6061': '#c9cdd6', 'Steel': '#8f96a3', 'PA 66': '#ece7d8', 'Bronze': '#c48a4c' };

const parts = [];      // every occurrence
const defs = {};       // definition name -> { name, count, material, volume, occurrences }

const clipLocal = new THREE.Plane(new THREE.Vector3(-1, 0, 0), 1e4);
const clipWorld = new THREE.Plane();
const CAP = new THREE.Color('#5a62e8');

const model = new THREE.Group();   // offset so the assembly is centred
const pivot = new THREE.Group();   // rotates
pivot.add(model);
model.position.x = -26;

function addPart(def, geom, color, pos, explode, opts = {}) {
  const group = new THREE.Group();
  const mat = new THREE.MeshStandardMaterial({
    color, metalness: opts.metal ?? 0.18, roughness: opts.rough ?? 0.52,
    clippingPlanes: [clipWorld], transparent: true,
  });
  const mesh = new THREE.Mesh(geom, mat);
  const cap = new THREE.Mesh(geom, new THREE.MeshBasicMaterial({ color: CAP, side: THREE.BackSide, clippingPlanes: [clipWorld], transparent: true }));
  const edges = new THREE.LineSegments(
    new THREE.EdgesGeometry(geom, 28),
    new THREE.LineBasicMaterial({ color: '#10121a', transparent: true, opacity: 0.55, clippingPlanes: [clipWorld] }),
  );
  group.add(cap, mesh, edges);
  group.position.copy(pos);
  model.add(group);
  const p = {
    def, group, mesh, cap, edges,
    home: pos.clone(), dir: explode.clone(),
    base: new THREE.Color(color), warm: new THREE.Color(opts.warm ?? color),
    order: opts.order ?? parts.length,
  };
  parts.push(p);
  defs[def] ??= { name: def, count: 0, material: opts.material ?? null, volume: opts.volume ?? 0, parts: [] };
  defs[def].count++;
  defs[def].parts.push(p);
  return p;
}

const V = (x, y, z) => new THREE.Vector3(x, y, z);
const cylX = (r, len, seg = 32) => new THREE.CylinderGeometry(r, r, len, seg).rotateZ(Math.PI / 2);
const cylY = (r, h, seg = 24) => new THREE.CylinderGeometry(r, r, h, seg);

// Top plate: a rounded 70 x 70 x 5 plate with a centre bore and four M5 clearance holes.
const PLATE = { w: 70, r: 6, t: 5, bore: 9, hole: 2.75, pitch: 25 };
function plateShape() {
  const { w, r } = PLATE, h = w / 2;
  const s = new THREE.Shape();
  s.moveTo(-h + r, -h); s.lineTo(h - r, -h); s.absarc(h - r, -h + r, r, -Math.PI / 2, 0);
  s.lineTo(h, h - r); s.absarc(h - r, h - r, r, 0, Math.PI / 2);
  s.lineTo(-h + r, h); s.absarc(-h + r, h - r, r, Math.PI / 2, Math.PI);
  s.lineTo(-h, -h + r); s.absarc(-h + r, -h + r, r, Math.PI, Math.PI * 1.5);
  const hole = (x, y, rr) => { const p = new THREE.Path(); p.absarc(x, y, rr, 0, Math.PI * 2, true); s.holes.push(p); };
  hole(0, 0, PLATE.bore);
  for (const [x, y] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) hole(x * PLATE.pitch, y * PLATE.pitch, PLATE.hole);
  return s;
}
const plateArea = (() => {
  const s = plateShape();
  const outer = Math.abs(THREE.ShapeUtils.area(s.getPoints(64)));
  const holes = s.holes.reduce((a, h) => a + Math.abs(THREE.ShapeUtils.area(h.getPoints(64))), 0);
  return outer - holes;
})();

function buildModel() {
  const vol = { box: (a, b, c) => a * b * c, cyl: (r, l) => Math.PI * r * r * l };
  addPart('Base Plate', new THREE.BoxGeometry(220, 8, 70), '#3d424d', V(0, 4, 0), V(0, -46, 0),
    { warm: '#3b3a40', material: 'Aluminium 6061', volume: vol.box(220, 8, 70), order: 0 });
  for (const s of [-1, 1]) addPart('End Block', new THREE.BoxGeometry(20, 50, 60), '#c3c7cf', V(100 * s, 33, 0), V(62 * s, 0, 0),
    { warm: '#e8c48f', material: 'Aluminium 6061', volume: vol.box(20, 50, 60), order: 1 });
  for (const s of [-1, 1]) addPart('Guide Rod', cylX(4, 184), '#d9dde4', V(0, 44, 20 * s), V(0, 8, 48 * s),
    { metal: 0.5, rough: 0.3, warm: '#f1eee8', material: 'Steel', volume: vol.cyl(4, 184), order: 2 });
  addPart('Lead Screw', cylX(3.5, 232, 24), '#b9bec8', V(16, 30, 0), V(0, -24, 0),
    { metal: 0.5, rough: 0.35, warm: '#d5cfc4', material: 'Steel', volume: vol.cyl(3.5, 232), order: 3 });
  addPart('Slider', new THREE.BoxGeometry(50, 24, 56), '#4048c8', V(0, 40, 0), V(0, 18, 0),
    { warm: '#d9643a', material: 'PA 66', volume: vol.box(50, 24, 56), order: 5 });
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) addPart('Bushing', cylX(6.5, 10, 24), '#c99a5f', V(30 * sx, 44, 20 * sz), V(34 * sx, 18, 0),
    { metal: 0.45, rough: 0.35, warm: '#c99a5f', material: 'Bronze', volume: vol.cyl(6.5, 10), order: 6 });
  const plateGeom = new THREE.ExtrudeGeometry(plateShape(), { depth: PLATE.t, bevelEnabled: false, curveSegments: 24 }).rotateX(-Math.PI / 2);
  addPart('Top Plate', plateGeom, '#c4c8d0', V(0, 52, 0), V(0, 58, 0),
    { warm: '#f2d9a6', material: 'Aluminium 6061', volume: plateArea * PLATE.t, order: 7 });
  for (const [x, z] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) addPart('M5 Screw', cylY(4, 3.5, 20), '#1b1c21', V(x * 25, 58.75, z * 25), V(0, 96, 0),
    { warm: '#1b1c21', order: 8 });
  for (const [x, z] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) addPart('M6 Screw', cylY(5, 2.5, 20), '#1b1c21', V(x * 70, 9.25, z * 27), V(0, -78, 0),
    { warm: '#1b1c21', order: 4 });
  addPart('Motor', new THREE.BoxGeometry(42, 42, 42), '#24262c', V(133, 37, 0), V(128, 0, 0), { warm: '#24262c', order: 9 });
  addPart('Motor', new THREE.BoxGeometry(8, 40, 40), '#8d929c', V(158, 37, 0), V(128, 0, 0), { warm: '#9a9286', order: 9 });
  defs.Motor.count = 1;
}
buildModel();

const plate = defs['Top Plate'].parts[0];
const trimmed = [...defs.Motor.parts, ...defs['M6 Screw'].parts];

// Dashed travel lines for the exploded view.
const travelGeom = new THREE.BufferGeometry();
travelGeom.setAttribute('position', new THREE.Float32BufferAttribute(new Float32Array(parts.length * 6), 3));
const travel = new THREE.LineSegments(travelGeom, new THREE.LineDashedMaterial({ color: '#8B93F8', dashSize: 3, gapSize: 3, transparent: true, opacity: 0 }));
model.add(travel);

// ---------------------------------------------------------------------------------------------
// Renderer
// ---------------------------------------------------------------------------------------------
let VW = viewportEl.clientWidth || 598, VH = viewportEl.clientHeight || 589;
let renderer = null;
const scene = new THREE.Scene();
scene.add(pivot);
scene.add(new THREE.HemisphereLight('#e4e9ff', '#1d2029', 1.9));
const sun = new THREE.DirectionalLight('#ffffff', 2.4); sun.position.set(220, 420, 300); scene.add(sun);
const fill = new THREE.DirectionalLight('#b8c2ff', 0.7); fill.position.set(-300, 80, -220); scene.add(fill);

const FRUSTUM = 250;
const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, -2000, 4000);
function setFrustum() {
  const a = VW / VH;
  camera.left = -FRUSTUM * a / 2; camera.right = FRUSTUM * a / 2;
  camera.top = FRUSTUM / 2; camera.bottom = -FRUSTUM / 2;
  camera.updateProjectionMatrix();
}
setFrustum();
const ISO = new THREE.Vector3(1, 0.82, 1.3).normalize();
const TOP = new THREE.Vector3(0.0001, 1, 0.0002).normalize();

try {
  renderer = new THREE.WebGLRenderer({ canvas: $('#gl'), antialias: true, alpha: true });
  renderer.localClippingEnabled = true;
  renderer.setClearColor(0x000000, 0);
  renderer.setSize(VW, VH, false);
} catch (e) {
  renderer = null; // No WebGL: the app chrome still renders; the viewport stays empty.
}

// ---------------------------------------------------------------------------------------------
// App chrome contents
// ---------------------------------------------------------------------------------------------
const TREE = [
  ['Linear Stage', 0, null, 'grp'], ['Base Plate', 1, 1], ['End Block', 1, 2], ['Guide Rod', 1, 2], ['Lead Screw', 1, 1],
  ['M6 Screw', 1, 4], ['Motor', 1, 1], ['Carriage', 1, null, 'grp'], ['Slider', 2, 1], ['Bushing', 2, 4], ['Top Plate', 2, 1], ['M5 Screw', 2, 4],
];
function fillTree(el) {
  el.innerHTML = TREE.map(([n, d, c, k]) => `<li class="${k || ''}" data-def="${n}" style="padding-left:${6 + d * 14}px">${n}${c ? ` <em>${c}×</em>` : ''}</li>`).join('');
}
fillTree($('#tree-inspect'));
fillTree($('#tree-drawing'));
const EXPORT_ORDER = ['Top Plate', 'Slider', 'End Block', 'Base Plate', 'Guide Rod', 'Lead Screw', 'Bushing', 'Motor'];
$('#tree-export').innerHTML = TREE.filter(t => !t[3]).map(([n, d, c]) => `<li data-def="${n}" style="padding-left:${6 + (d - 1) * 14}px">${n} <em>${c}×</em></li>`).join('');
$('#bucket').innerHTML = EXPORT_ORDER.map(n => `<li data-def="${n}"><svg><use href="#i-file"/></svg><span>${n}.step</span><small>Ready</small></li>`).join('');
$('#pop-files').innerHTML = EXPORT_ORDER.map(n => `<div class="fchip"><svg><use href="#i-file"/></svg>${n}.step <small>written</small></div>`).join('');

const MASS_ROWS = Object.values(defs).filter(d => d.material).map(d => ({
  ...d, grams: d.volume / 1000 * DENSITY[d.material] * d.count,
}));
const MASS_TOTAL = MASS_ROWS.reduce((a, r) => a + r.grams, 0);
$('#mats').innerHTML = MASS_ROWS.map(r => `<li><i style="background:${MAT_COLOR[r.material]}"></i><span>${r.name}${r.count > 1 ? ` ×${r.count}` : ''}</span><em>${r.material}</em></li>`).join('');

// 2D slice at X = 0 through the carriage, in the YZ plane, drawn from the same dimensions.
(function drawSlice() {
  const Y = y => 60 - y;
  const rect = (z0, z1, y0, y1, c) => `<rect x="${z0}" y="${Y(y1)}" width="${z1 - z0}" height="${y1 - y0}" fill="${c}" stroke="#F2F4F9"/>`;
  const circ = (z, y, r, c) => `<circle cx="${z}" cy="${Y(y)}" r="${r}" fill="${c}" stroke="#F2F4F9"/>`;
  $('#slice-svg').innerHTML = [
    rect(-35, 35, 0, 8, '#4a5166'),
    rect(-28, 28, 28, 52, '#5a62e8'),
    circ(-20, 44, 4, '#9aa2b3'), circ(20, 44, 4, '#9aa2b3'),
    circ(0, 30, 3.5, '#9aa2b3'),
    rect(-35, -9, 52, 57, '#c3c8d2'), rect(9, 35, 52, 57, '#c3c8d2'),
    `<path d="M-35 ${Y(-3)}h70M-35 ${Y(-1.5)}v-3M35 ${Y(-1.5)}v-3" stroke="#FFD166"/><text x="0" y="${Y(-8)}" text-anchor="middle" fill="#FFD166" stroke="none" font-size="3.6" font-family="Archivo">70.00</text>`,
  ].join('');
})();

// Plate outline for the Drawing workspace: the 2D preview and the big pop share one path.
const plateOutline = (() => {
  const { w, r, bore, hole, pitch } = PLATE, h = w / 2;
  const outer = `M${-h + r} ${-h}H${h - r}A${r} ${r} 0 0 1 ${h} ${-h + r}V${h - r}A${r} ${r} 0 0 1 ${h - r} ${h}H${-h + r}A${r} ${r} 0 0 1 ${-h} ${h - r}V${-h + r}A${r} ${r} 0 0 1 ${-h + r} ${-h}Z`;
  const circle = (x, y, rr) => `M${x - rr} ${y}a${rr} ${rr} 0 1 0 ${rr * 2} 0a${rr} ${rr} 0 1 0 ${-rr * 2} 0Z`;
  const holes = [circle(0, 0, bore), ...[[-1, -1], [1, -1], [1, 1], [-1, 1]].map(([x, y]) => circle(x * pitch, y * pitch, hole))];
  return [outer, ...holes];
})();
$('#pv2d').innerHTML = plateOutline.map(d => `<path d="${d}" pathLength="1"/>`).join('');
$('#outline-svg').innerHTML = plateOutline.map(d => `<path d="${d}" pathLength="1" stroke="#F2F4F9" stroke-width="0.7"/>`).join('') + `
  <g class="dims" stroke="#FFD166" stroke-width="0.4" fill="none">
    <path d="M-35 41v5M35 41v5M-35 44h70" pathLength="1"/>
    <text x="0" y="42.5" text-anchor="middle" stroke="none" fill="#FFD166" font-size="3.8" font-family="Archivo">70.00</text>
    <path d="M41 -35h5M41 35h5M44 -35v70" pathLength="1"/>
    <text x="47" y="1.4" stroke="none" fill="#FFD166" font-size="3.8" font-family="Archivo">70.00</text>
    <path d="M6.4 -6.4 18 -18h10" pathLength="1"/>
    <text x="19" y="-19.5" stroke="none" fill="#FFD166" font-size="3.8" font-family="Archivo">Ø18.00</text>
  </g>`;

// ---------------------------------------------------------------------------------------------
// State, and how it is drawn
// ---------------------------------------------------------------------------------------------
const S = {
  k: 0, heroOut: 0, spin: 1, rot: 0.35,
  open: 0, race: 0,
  warm: 0, ghost: 0, checker: 0, capture: 0, png: 0,
  exWin: 0, explode: 0,
  sec: 0, secPos: 160, slice: 0,
  measure: 0, mass: 0,
  ws: 0, split: 0, files: 0, trim: 0, onefile: 0,
  top: 0, iso: 0, outline: 0,
  tools: '',
};

const tmpC = new THREE.Color(), tmpV = new THREE.Vector3(), tmpW = new THREE.Vector3();
const ACCENT = new THREE.Color('#8B93F8');
let t0 = performance.now();

function apply(now = performance.now()) {
  const t = (now - t0) / 1000;

  // --- model pose ---
  const idle = S.spin * t * 0.3 + (1 - S.spin) * Math.sin(t * 0.45) * 0.14 * (1 - S.top);
  pivot.rotation.y = S.rot + idle;

  const visibleCount = S.open <= 0 ? parts.length : Math.floor(parts.length * seg(S.open, 0.08, 0.92));
  const byOrder = [...parts].sort((a, b) => a.order - b.order);
  const shown = new Set(byOrder.slice(0, visibleCount));
  const splitIdx = Math.floor(S.split * (EXPORT_ORDER.length + 0.999)) - 1; // part being ticked
  const splitDef = splitIdx >= 0 && splitIdx < EXPORT_ORDER.length && S.split < 1 ? EXPORT_ORDER[splitIdx] : null;
  const pos = travelGeom.attributes.position.array;

  parts.forEach((p, i) => {
    // explode, trim drop
    const isTrim = trimmed.includes(p);
    const drop = isTrim ? S.trim : 0;
    p.group.position.copy(p.home).addScaledVector(p.dir, S.explode);
    p.group.position.y -= drop * 30;
    pos.set([p.home.x, p.home.y, p.home.z, p.group.position.x, p.group.position.y, p.group.position.z], i * 6);

    // colour: base -> warm (present) -> material (mass)
    tmpC.copy(p.base).lerp(p.warm, S.warm);
    const d = defs[p.def];
    if (d.material) tmpC.lerp(new THREE.Color(MAT_COLOR[d.material]), S.mass);
    const hi = (p.def === splitDef ? 0.55 : 0) + (p === plate ? seg(S.measure, 0, 0.18) * (1 - seg(S.measure, 0.3, 0.5)) * 0.5 : 0);
    if (hi) tmpC.lerp(ACCENT, hi);
    p.mesh.material.color.copy(tmpC);

    // opacity: ghosted housings (present), isolate plate (drawing), trimmed parts
    let a = 1;
    if (p.def === 'End Block' || p.def === 'Motor') a = Math.min(a, 1 - S.ghost * 0.72);
    if (p !== plate) a = Math.min(a, 1 - S.iso);
    a = Math.min(a, 1 - drop);
    const appear = shown.has(p) ? 1 : 0;
    a *= appear;
    p.group.visible = a > 0.01;
    p.mesh.material.opacity = a;
    p.mesh.material.depthWrite = a > 0.98;
    p.cap.material.opacity = a;
    p.cap.visible = S.sec > 0.01;
    p.edges.material.opacity = 0.55 * a;
  });
  travelGeom.attributes.position.needsUpdate = true;
  travel.computeLineDistances();
  travel.material.opacity = 0.75 * seg(S.explode, 0.05, 0.4);

  // section plane follows the model as it turns
  clipLocal.constant = S.sec > 0.01 ? S.secPos : 1e4;
  pivot.updateMatrixWorld(true);
  clipWorld.copy(clipLocal).applyMatrix4(model.matrixWorld);

  // camera: iso -> top view of the plate for the Drawing workspace
  tmpV.copy(ISO).lerp(TOP, S.top).normalize();
  tmpW.set(0, 32, 0);
  if (S.sec > 0) tmpW.lerp(new THREE.Vector3(-58, 34, 0).applyMatrix4(model.matrixWorld), S.sec); // frame the kept half
  if (S.top > 0 || S.iso > 0) {
    const plateWorld = plate.group.getWorldPosition(new THREE.Vector3()).add(V(0, 2.5, 0));
    tmpW.lerp(plateWorld, Math.max(S.top, S.iso));
  }
  camera.position.copy(tmpW).addScaledVector(tmpV, 800);
  camera.up.set(0, 1 - S.top, -S.top).normalize();
  camera.lookAt(tmpW);
  camera.zoom = lerp(1, 2.55, S.top) * lerp(1, 0.78, S.explode);
  camera.updateProjectionMatrix();

  if (renderer && renderActive) renderer.render(scene, camera);

  drawOverlay();
  syncChrome();
}

// Measurement dimensions, projected from model points into the viewport each frame.
const MEAS = [
  { a: V(-9, 57, 0), b: V(9, 57, 0), label: 'Ø 18.00 mm', from: 0.2, to: 0.55 },
  { a: V(-62, 44, -20), b: V(-62, 44, 20), label: '40.00 mm', from: 0.5, to: 0.85 },
];
function project(v) {
  tmpV.copy(v).applyMatrix4(model.matrixWorld).project(camera);
  return [(tmpV.x + 1) / 2 * VW, (1 - tmpV.y) / 2 * VH];
}
function drawOverlay() {
  if (S.measure <= 0.01) { if (overlay.childElementCount) overlay.innerHTML = ''; return; }
  overlay.setAttribute('viewBox', `0 0 ${VW} ${VH}`);
  let out = '';
  for (const m of MEAS) {
    const k = seg(S.measure, m.from, m.to);
    if (k <= 0) continue;
    const [ax, ay] = project(m.a), [bx, by] = project(m.b);
    const ex = lerp(ax, bx, k), ey = lerp(ay, by, k);
    out += `<g stroke="#FFD166" stroke-width="2"><circle cx="${ax}" cy="${ay}" r="3.5" fill="#FFD166"/><line x1="${ax}" y1="${ay}" x2="${ex}" y2="${ey}"/>${k > 0.98 ? `<circle cx="${bx}" cy="${by}" r="3.5" fill="#FFD166"/>` : ''}</g>`;
    if (k > 0.9) {
      const mx = (ax + bx) / 2, my = (ay + by) / 2 - 30;
      const w = m.label.length * 7.2 + 16;
      out += `<g opacity="${seg(k, 0.9, 1)}"><line x1="${(ax + bx) / 2}" y1="${(ay + by) / 2}" x2="${mx}" y2="${my + 11}" stroke="#FFD166" stroke-width="1.2"/><rect x="${mx - w / 2}" y="${my - 11}" width="${w}" height="22" rx="5" fill="#FFD166"/><text x="${mx}" y="${my + 4}" text-anchor="middle">${m.label}</text></g>`;
    }
  }
  overlay.innerHTML = out;
}

// Chrome: workspace tabs, toolbar, tool windows, panels and page-scale callouts.
const toolEls = Object.fromEntries($$('#toolbar [data-tool]').map(e => [e.dataset.tool, e]));
const tabEls = $$('#ws-seg [data-tab]');
const railCards = $$('#rail .card');
const treeExport = $$('#tree-export li');
const treeInspect = $$('#tree-inspect li');
const bucketRows = $$('#bucket li');
const matRows = $$('#mats li');
const fileChips = $$('#pop-files .fchip');
const outlinePaths = $$('#outline-svg path');
const pvPaths = $$('#pv2d path');
const vis = (el, v) => { el.style.opacity = v; el.style.visibility = v > 0.001 ? 'visible' : 'hidden'; };
let last = {};

function syncChrome() {
  // workspace strip
  const ws = S.ws;
  $('#slide-l').style.transform = $('#slide-r').style.transform = `translateX(${-ws * 33.3333}%)`;
  const wsIdx = Math.round(ws);
  if (last.ws !== wsIdx) {
    tabEls.forEach((e, i) => e.classList.toggle('on', i === wsIdx));
    railCards.forEach((e, i) => e.classList.toggle('on', i === wsIdx));
    app.dataset.ws = ['inspect', 'export', 'drawing'][wsIdx];
    $('#vp-label').textContent = ['', 'Master assembly · parts ticked for export', 'Top Plate · top view'][wsIdx];
    last.ws = wsIdx;
  }
  const a = Math.floor(ws), f = ws - a;
  const tw = tabRects[a], tb = tabRects[Math.min(a + 1, 2)];
  if (tw) { const ind = $('#seg-ind'); ind.style.transform = `translateX(${lerp(tw.x, tb.x, f)}px)`; ind.style.width = `${lerp(tw.w, tb.w, f)}px`; }
  if (railCards.length) $('#rail-track').style.transform = `translateX(${railShift(ws)}px)`;
  vis($('#vp-label'), clamp(ws * 2));
  vis($('#toolbar'), 1 - clamp(ws * 2));

  // loading
  const opening = S.open > 0 && S.open < 1;
  if (opening) { $('#vp-label').textContent = `Opening linear-stage.step… ${Math.round(seg(S.open, 0.08, 0.92) * 100)}%`; vis($('#vp-label'), 1); last.ws = -1; }
  $$('#props-doc [data-count]').forEach(e => {
    const target = +e.dataset.count;
    const k = S.open <= 0 ? 1 : seg(S.open, 0.1, 0.4 + target / 1400);
    e.textContent = k <= 0 ? '—' : `${Math.round(target * k)} ms`;
  });

  // toolbar highlights
  const on = new Set();
  if (S.ghost > 0.3) on.add('transparent');
  if (S.capture > 0.3) on.add('capture');
  if (S.exWin > 0.3) on.add('explode');
  if (S.sec > 0.3) on.add('section');
  if (S.measure > 0.05) on.add('measure');
  if (S.trim > 0.05 && S.trim < 1) on.add('hide');
  for (const [k, e] of Object.entries(toolEls)) e.classList.toggle('on', on.has(k));

  // tool windows
  vis($('#tw-capture'), S.capture);
  vis($('#tw-explode'), S.exWin);
  vis($('#tw-section'), S.sec);
  vis($('#tw-measure'), seg(S.measure, 0, 0.2));
  $('#ex-fill').style.width = `${S.explode * 100}%`;
  $('#ex-val').textContent = `${Math.round(S.explode * 100)}%`;
  $('#sec-pos').textContent = (S.secPos - 0).toFixed(1);
  $('#sw-slice').classList.toggle('on', S.slice > 0.5);
  vis($('#vp-checker'), S.checker);

  // right panel: document props <-> materials and mass
  vis($('#props-doc'), 1 - clamp(S.mass * 4));
  vis($('#props-mass'), clamp(S.mass * 4));
  $('#props-title').textContent = S.mass > 0.12 ? 'Selection properties' : 'Document properties';
  const matOn = Math.floor(seg(S.mass, 0.1, 0.7) * matRows.length + 0.0001);
  matRows.forEach((e, i) => e.classList.toggle('on', i < matOn));
  const grams = MASS_TOTAL * seg(S.mass, 0.15, 0.85);
  $('#mass-total').textContent = `${grams.toLocaleString('en-US', { minimumFractionDigits: 1, maximumFractionDigits: 1 })} g`;
  $('#pop-mass-v').textContent = Math.round(grams).toLocaleString('en-US');

  // export: ticks and bucket
  const ticked = Math.floor(S.split * (EXPORT_ORDER.length + 0.999));
  const tickedSet = new Set(EXPORT_ORDER.slice(0, ticked));
  treeExport.forEach(e => {
    e.classList.toggle('on', tickedSet.has(e.dataset.def));
    e.classList.toggle('gone', S.trim > 0.4 && (e.dataset.def === 'Motor' || e.dataset.def === 'M6 Screw'));
  });
  treeInspect.forEach(e => e.classList.toggle('sel', (S.measure > 0.05 && e.dataset.def === 'Top Plate') || (S.iso > 0.5 && e.dataset.def === 'Top Plate')));
  $$('#tree-drawing li').forEach(e => e.classList.toggle('sel', e.dataset.def === 'Top Plate' && S.ws > 1.5));
  bucketRows.forEach((e, i) => { e.style.display = i < ticked ? 'flex' : 'none'; });
  $('#bucket-n').textContent = `${ticked} file${ticked === 1 ? '' : 's'}`;
  $('#export-btn').textContent = S.trim > 0.4 ? 'Export assembly without hidden parts…' : `Export ${ticked || ''} files`.replace('  ', ' ');

  // drawing preview
  const draw = seg(S.outline, 0, 0.6);
  pvPaths.forEach(p => { p.style.strokeDasharray = '1'; p.style.strokeDashoffset = `${1 - draw}`; });
  outlinePaths.forEach((p, i) => { const k = i < plateOutline.length ? draw : seg(S.outline, 0.55, 1); p.style.strokeDasharray = '1'; p.style.strokeDashoffset = `${1 - k}`; });
  $$('#outline-svg text').forEach(e => e.style.opacity = seg(S.outline, 0.75, 1));

  // pops
  vis($('#pop-race'), S.race); $('#pop-race').style.setProperty('--race', seg(S.race, 0.2, 1));
  vis($('#pop-png'), S.png);
  $('#pop-png').style.transform = `translate(${(1 - S.png) * 40}px, ${(1 - S.png) * -30}px) rotate(${lerp(6, -3.5, S.png)}deg) scale(${lerp(0.85, 1, S.png)})`;
  if (S.png > 0.02 && !pngTaken) takePng();
  vis($('#pop-slice'), S.slice); $('#pop-slice').style.transform = `translateY(${(1 - S.slice) * 24}px)`;
  vis($('#pop-mass'), seg(S.mass, 0.05, 0.3));
  vis($('#pop-files'), S.files > 0 ? 1 : 0);
  fileChips.forEach((e, i) => {
    const k = seg(S.files, i / fileChips.length * 0.7, i / fileChips.length * 0.7 + 0.3);
    e.style.opacity = k;
    e.style.transform = `translate(${lerp(320, (i % 2) * 236, k)}px, ${lerp(-60, Math.floor(i / 2) * 50, k)}px) rotate(${lerp(10, 0, k)}deg)`;
  });
  vis($('#pop-onefile'), S.onefile); $('#pop-onefile').style.transform = `translateY(${(1 - S.onefile) * 20}px)`;
  vis($('#pop-outline'), seg(S.outline, 0, 0.1));

  // hero copy
  if (!motion) return;
  const ho = $('#hero-copy');
  ho.style.opacity = 1 - S.heroOut; ho.style.visibility = S.heroOut > 0.99 ? 'hidden' : 'visible';
  ho.style.transform = `translate(-50%, ${-S.heroOut * 70}px)`;
}

let pngTaken = false;
function takePng() {
  if (!renderer) return;
  pngTaken = true;
  renderer.render(scene, camera);
  const img = new Image();
  img.alt = 'The capture: the linear stage on a transparent background.';
  img.src = renderer.domElement.toDataURL('image/png');
  $('#png-slot').replaceChildren(img);
}

// ---------------------------------------------------------------------------------------------
// Layout: hero laptop -> screen fills the window -> app beside the copy
// ---------------------------------------------------------------------------------------------
let R0, R1, R2, rigBox, tabRects = [], railStep = 0, appScale = 0.5, effScale = 0.5;
const narrow = () => innerWidth < 900;

function measure() {
  rig.style.transform = 'none';
  const vw = innerWidth, vh = innerHeight, nav = 52;
  const hero = $('#hero-copy');
  if (motion) rig.style.setProperty('--rig-top', `${hero.offsetTop + hero.offsetHeight + Math.max(28, vh * 0.04)}px`);
  app.classList.toggle('compact', narrow());
  VW = viewportEl.clientWidth; VH = viewportEl.clientHeight; setFrustum();
  if (renderer) renderer.setSize(VW, VH, false);

  const sr = screenEl.getBoundingClientRect(), rr = rig.getBoundingClientRect(), tr = theater.getBoundingClientRect();
  rigBox = { x: rr.left - tr.left, y: rr.top - tr.top };
  R0 = { x: sr.left - tr.left, y: sr.top - tr.top, w: sr.width };
  appScale = sr.width / 1080;
  app.style.setProperty('--as', appScale);
  if (renderer && !motion) { renderer.setPixelRatio(Math.min(3, Math.max(1, devicePixelRatio * appScale))); renderer.setSize(VW, VH, false); }
  effScale = 0.001; // force a buffer resize on the next placeRig()

  // R1: the screen fills the window
  const w1 = Math.min(vw * 0.94, (vh - nav - 28) * 1.6);
  R1 = { x: (vw - w1) / 2, y: nav + (vh - nav - w1 / 1.6) / 2, w: w1 };

  // R2: the stage position
  let cx, cy, cw;
  if (!narrow()) {
    const railH = 64 + 18;
    const w2 = Math.min(vw * 0.56, (vh - nav - railH - 56) * 1.6, 1500);
    const x2 = vw - w2 - Math.max(24, vw * 0.035);
    const y2 = nav + (vh - nav - (w2 / 1.6 + railH)) / 2;
    R2 = { x: x2, y: y2, w: w2 };
    cx = Math.max(24, vw * 0.05); cw = x2 - cx - Math.max(32, vw * 0.04); cy = y2 + 8;
  } else {
    const w2 = vw - 24;
    R2 = { x: 12, y: nav + 12, w: w2 };
    cx = 20; cw = vw - 40; cy = R2.y + w2 / 1.6 + 22;
  }
  const s = theater.style;
  s.setProperty('--ax', `${R2.x}px`); s.setProperty('--ay', `${R2.y}px`);
  s.setProperty('--aw', `${R2.w}px`); s.setProperty('--ah', `${R2.w / 1.6}px`);
  s.setProperty('--cx', `${cx}px`); s.setProperty('--cy', `${cy}px`); s.setProperty('--cw', `${cw}px`);
  const ch = Math.max(...$$('.sc').map(e => e.offsetHeight)) + (narrow() ? 14 : 32);
  s.setProperty('--ch', `${ch}px`);

  // tab indicator and rail geometry (logical px inside the app)
  tabRects = tabEls.map(e => ({ x: e.offsetLeft - 2, w: e.offsetWidth }));
  const card = railCards[0];
  railStep = card ? card.offsetWidth + 12 : 0;
  placeRig();
}
function railShift(ws) {
  const w = R2 ? R2.w : 0;
  return w / 2 - ws * railStep - (railStep - 12) / 2; // centre the active card
}

function rectAt(k) {
  if (k <= 1) { const e = k; return { x: lerp(R0.x, R1.x, e), y: lerp(R0.y, R1.y, e), w: lerp(R0.w, R1.w, e) }; }
  const e = k - 1; return { x: lerp(R1.x, R2.x, e), y: lerp(R1.y, R2.y, e), w: lerp(R1.w, R2.w, e) };
}
function placeRig() {
  if (!motion || !R0) return;
  const r = rectAt(S.k);
  const s = r.w / R0.w;
  const tx = r.x - rigBox.x - s * (R0.x - rigBox.x);
  const ty = r.y - rigBox.y - s * (R0.y - rigBox.y);
  rig.style.transform = `translate(${tx}px, ${ty}px) scale(${s})`;
  rig.style.setProperty('--frame', (1 - seg(S.k, 0.45, 1.05)).toFixed(3));
  vis($('#rail'), seg(S.k, 1.6, 2));
  // keep the WebGL buffer sharp at whatever size the screen is shown
  const eff = appScale * s;
  if (renderer && Math.abs(eff - effScale) / effScale > 0.08) {
    effScale = eff;
    renderer.setPixelRatio(Math.min(3, Math.max(1, devicePixelRatio * eff)));
    renderer.setSize(VW, VH, false);
  }
}

// ---------------------------------------------------------------------------------------------
// The scroll story
// ---------------------------------------------------------------------------------------------
const SCENES = ['speed', 'present', 'explode', 'section', 'measure', 'mass', 'split', 'trim', 'drawing'];
const T = { speed: 1.75, present: 3.1, explode: 4.4, section: 5.7, measure: 7.0, mass: 8.3, split: 9.9, trim: 11.2, drawing: 12.8, end: 14.4 };

function buildTimeline() {
  theater.classList.add('motion');
  document.documentElement.classList.add('motion');
  const tl = gsap.timeline({ defaults: { ease: 'none' }, paused: true });
  const to = (vars, at, dur = 0.3, ease = 'power2.inOut') => tl.to(S, { ...vars, duration: dur, ease }, at);

  // hero -> screen -> stage
  to({ heroOut: 1 }, 0, 0.45, 'power1.in');
  to({ k: 1 }, 0, 1.0, 'power1.inOut');
  to({ k: 2, spin: 0 }, 1.0, 0.65, 'power2.inOut');

  // copy blocks
  const blocks = $$('.sc');
  SCENES.forEach((name, i) => {
    const el = blocks[i];
    const tIn = T[name] - 0.1;
    const next = T[SCENES[i + 1]] ?? T.end + 1;
    tl.fromTo(el, { autoAlpha: 0, y: 28 }, { autoAlpha: 1, y: 0, duration: 0.25, ease: 'power2.out' }, tIn);
    if (i < SCENES.length - 1) tl.to(el, { autoAlpha: 0, y: -22, duration: 0.18, ease: 'power1.in' }, next - 0.3);
  });

  // speed: reopen the file
  tl.set(S, { open: 0.001 }, T.speed);
  to({ open: 1 }, T.speed + 0.05, 0.9, 'power1.out');
  to({ race: 1 }, T.speed + 0.25, 0.8, 'power2.out');
  to({ race: 0 }, T.present - 0.3, 0.2);
  tl.set(S, { open: 0 }, T.present - 0.05);

  // present
  to({ warm: 1 }, T.present, 0.35);
  to({ ghost: 1 }, T.present + 0.25, 0.3);
  to({ capture: 1 }, T.present + 0.35, 0.15);
  to({ checker: 1 }, T.present + 0.5, 0.25);
  to({ png: 1 }, T.present + 0.75, 0.35, 'power3.out');
  to({ png: 0, capture: 0, checker: 0, ghost: 0 }, T.explode - 0.3, 0.2);

  // explode
  to({ exWin: 1 }, T.explode, 0.15);
  to({ explode: 1, rot: 0.95 }, T.explode + 0.1, 0.75, 'power2.inOut');
  to({ explode: 0 }, T.section - 0.4, 0.3);
  to({ exWin: 0 }, T.section - 0.2, 0.12);

  // section: sweep in from the motor end, stop at the carriage, then lay the slice flat
  to({ sec: 1 }, T.section, 0.12);
  to({ secPos: 0, rot: -0.45 }, T.section + 0.05, 0.6, 'power2.out');
  to({ slice: 1 }, T.section + 0.7, 0.3, 'power3.out');
  to({ slice: 0, sec: 0 }, T.measure - 0.25, 0.18);
  tl.set(S, { secPos: 160 }, T.measure - 0.05);

  // measure
  to({ measure: 1 }, T.measure + 0.05, 0.7, 'none');
  to({ measure: 0 }, T.mass - 0.2, 0.12);

  // mass
  to({ mass: 1 }, T.mass + 0.05, 0.75, 'power1.inOut');
  to({ mass: 0 }, T.split - 0.55, 0.2);

  // -> Export workspace (horizontal)
  to({ ws: 1 }, T.split - 0.35, 0.35, 'power3.inOut');
  to({ split: 1 }, T.split + 0.05, 0.75, 'none');
  to({ files: 1 }, T.split + 0.55, 0.55, 'power2.out');
  to({ files: 0 }, T.trim - 0.25, 0.15);

  to({ trim: 1 }, T.trim + 0.05, 0.5, 'power2.in');
  to({ onefile: 1 }, T.trim + 0.6, 0.25, 'power3.out');
  to({ onefile: 0 }, T.drawing - 0.45, 0.15);

  // -> Drawing workspace (horizontal)
  to({ ws: 2, trim: 0 }, T.drawing - 0.35, 0.35, 'power3.inOut');
  to({ iso: 1, rot: 0 }, T.drawing, 0.4);
  to({ top: 1 }, T.drawing + 0.15, 0.45, 'power2.inOut');
  to({ outline: 1 }, T.drawing + 0.55, 0.85, 'power1.inOut');
  tl.to({}, { duration: 0.6 }, T.end);

  const UNIT = () => innerHeight * 0.7;
  ScrollTrigger.create({
    trigger: theater,
    start: 'top top',
    end: () => `+=${Math.round(tl.duration() * UNIT())}`,
    pin: true,
    scrub: 0.6,
    animation: tl,
    invalidateOnRefresh: true,
    onRefresh: () => { measure(); apply(); },
  });
  tl.eventCallback('onUpdate', () => { placeRig(); });
}

// Fallback: no scrubbing. Each scene's state is set when its text comes into view.
const STILL = {
  speed: {}, present: { warm: 1, ghost: 1, capture: 1, checker: 1 }, explode: { exWin: 1, explode: 1, rot: 0.95 },
  section: { sec: 1, secPos: 0, slice: 1, rot: -0.45 }, measure: { measure: 1 }, mass: { mass: 1 },
  split: { ws: 1, split: 1 }, trim: { ws: 1, split: 1, trim: 1 }, drawing: { ws: 2, iso: 1, top: 1, outline: 1, rot: 0 },
};
const RESET = { ...S, k: 0, heroOut: 0, spin: 0 };
function buildStills() {
  const io = new IntersectionObserver(entries => {
    for (const e of entries) if (e.isIntersecting) Object.assign(S, RESET, STILL[e.target.dataset.scene] || {});
  }, { rootMargin: '-45% 0px -45% 0px' });
  $$('.sc').forEach(el => io.observe(el));
}

// ---------------------------------------------------------------------------------------------
// Hero rotating line
// ---------------------------------------------------------------------------------------------
function rotator() {
  const lines = $$('#rot-track .rot-line');
  if (!motion || lines.length < 2) return;
  gsap.set(lines, { autoAlpha: 0, yPercent: 100 });
  gsap.set(lines[0], { autoAlpha: 1, yPercent: 0 });
  let i = 0;
  setInterval(() => {
    if (S.heroOut > 0.5 || document.hidden) return;
    const a = lines[i], b = lines[(i + 1) % lines.length];
    gsap.to(a, { autoAlpha: 0, yPercent: -100, duration: 0.45, ease: 'power2.in' });
    gsap.fromTo(b, { autoAlpha: 0, yPercent: 100 }, { autoAlpha: 1, yPercent: 0, duration: 0.6, ease: 'power3.out', delay: 0.2 });
    i = (i + 1) % lines.length;
  }, 2600);
}

// ---------------------------------------------------------------------------------------------
// Render loop: only while the theatre is on screen
// ---------------------------------------------------------------------------------------------
let renderActive = true;
new IntersectionObserver(([e]) => { renderActive = e.isIntersecting; }).observe(theater);
function loop(now) {
  if (renderActive && !document.hidden) apply(now);
  requestAnimationFrame(loop);
}

// ---------------------------------------------------------------------------------------------
// Downloads: point each button at its zip in the latest release.
// ---------------------------------------------------------------------------------------------
function downloads() {
  const buttons = $$('[data-downloads] .dl');
  const ua = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || navigator.userAgent;
  const mine = /win/i.test(ua) ? 'windows-x64' : 'macos-arm64';
  buttons.forEach(b => { if (b.dataset.asset === mine) b.classList.add('primary'); });
  fetch('https://api.github.com/repos/imperator28/Loupe/releases/latest', { headers: { Accept: 'application/vnd.github+json' } })
    .then(r => { if (!r.ok) throw new Error(r.status); return r.json(); })
    .then(rel => {
      buttons.forEach(b => {
        const suffix = `-${b.dataset.asset}.zip`;
        const asset = (rel.assets || []).find(a => a.name.endsWith(suffix));
        if (asset) b.href = asset.browser_download_url;
      });
      if (rel.tag_name) $$('[data-version]').forEach(e => { e.textContent = `Loupe ${rel.tag_name}`; });
      if (rel.html_url) $$('[data-notes]').forEach(e => { e.href = rel.html_url; });
    })
    .catch(() => {});
}

// ---------------------------------------------------------------------------------------------
downloads();
if (motion) {
  gsap.registerPlugin(ScrollTrigger);
  measure();
  buildTimeline();
  rotator();
  addEventListener('resize', () => ScrollTrigger.refresh());
  document.fonts?.ready.then(() => ScrollTrigger.refresh());
} else {
  S.spin = 0;
  measure();
  buildStills();
  addEventListener('resize', measure);
}
requestAnimationFrame(loop);
