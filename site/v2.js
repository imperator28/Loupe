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

// The laptop blueprint traced on load, drawn from the same corners as the CSS laptop: superellipse
// (squircle, G2) corners where the browser supports corner-shape, otherwise circular ones. In the
// drawing's units (1000 wide), y down. Each corner is sampled finely; at these sizes the polyline is
// indistinguishable from the curve.
(function macBlueprint() {
  const paths = $$('.bp-lines .bp-o path');
  if (paths.length < 5) return;
  const SQ = CSS.supports('corner-shape', 'squircle');
  let e = SQ ? 0.5 : 1; // superellipse x^4 + y^4 = 1 is (cos t)^(1/2), (sin t)^(1/2); exponent 1 is the circle
  const N = 24, f = v => +v.toFixed(2);
  const pt = (cx, cy, rx, ry, deg) => {
    const a = deg * Math.PI / 180, c = Math.cos(a), s = Math.sin(a);
    return [cx + rx * Math.sign(c) * Math.abs(c) ** e, cy + ry * Math.sign(s) * Math.abs(s) ** e];
  };
  const arc = (cx, cy, rx, ry, from, to) => {
    let d = '';
    for (let i = 1; i <= N; i++) { const [x, y] = pt(cx, cy, rx, ry, from + (to - from) * i / N); d += `L${f(x)} ${f(y)}`; }
    return d;
  };
  // rounded rectangle, corners as [rx, ry]: top-left, top-right, bottom-right, bottom-left
  const rrect = (x, y, w, h, tl, tr, br, bl) =>
    `M${f(x + tl[0])} ${f(y)}H${f(x + w - tr[0])}` + arc(x + w - tr[0], y + tr[1], tr[0], tr[1], -90, 0)
    + `V${f(y + h - br[1])}` + arc(x + w - br[0], y + h - br[1], br[0], br[1], 0, 90)
    + `H${f(x + bl[0])}` + arc(x + bl[0], y + h - bl[1], bl[0], bl[1], 90, 180)
    + `V${f(y + tl[1])}` + arc(x + tl[0], y + tl[1], tl[0], tl[1], 180, 270) + 'Z';
  const R = SQ ? { lid: [30, 30], screen: [10, 10], base: [18, 18] }
               : { lid: [21.3, 21.9], screen: [7.4, 7.75], base: [13, 13] };
  const deck = [2, 2], pill = [4.68, 4.68];
  paths[0].setAttribute('d', rrect(74.5, 0, 851, 561.1, R.lid, R.lid, R.lid, R.lid));          // lid
  paths[1].setAttribute('d', rrect(86.5, 12, 827, 534.6, R.screen, R.screen, R.screen, R.screen)); // screen
  // camera: .cam is 2.7% x 1.75% of the screen (827 x 534.6), 0.6% down, centred. A true pill with
  // semicircular ends: a squircle with radius = half the height reads as a rounded rectangle.
  const shape = e;
  e = 1;
  paths[2].setAttribute('d', rrect(500 - 11.165, 12 + 3.21, 22.33, 9.36, pill, pill, pill, pill));
  e = shape;
  paths[3].setAttribute('d', rrect(0, 561.1, 1000, 33, deck, deck, R.base, R.base));           // base
  // the thumb scoop: open at the deck, 10 across and its full depth down at each corner
  const sx = 427.5, sw = 145, sy = 561.1, sh = 11.88, rx = 10;
  paths[4].setAttribute('d', `M${sx} ${sy}` + arc(sx + rx, sy, rx, sh, 180, 90) + `H${f(sx + sw - rx)}` + arc(sx + sw - rx, sy, rx, sh, 90, 0));
})();
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
// The app's built-in material library: name, density (g/cm³), display colour.
const LIBRARY = [
  ['Aluminum 6061', 2.70, '#B8C2CC'], ['Aluminum casting', 2.68, '#9EABB7'], ['Carbon steel', 7.85, '#6F7E8B'],
  ['Stainless steel 304', 8.00, '#C7D0D8'], ['Zinc alloy', 6.60, '#8A929A'], ['Copper', 8.96, '#C9825D'],
  ['ABS', 1.04, '#D1A45B'], ['Polypropylene (PP)', 0.90, '#B8B8AD'], ['HDPE', 0.95, '#D9E3E4'],
  ['Polycarbonate (PC)', 1.20, '#8596A6'], ['Polyamide (PA / nylon)', 1.14, '#D3C6A9'], ['PMMA (acrylic)', 1.18, '#BFD8E6'],
  ['PCBA (FR-4)', 1.85, '#3A7A58'], ['Rubber (HNBR)', 1.20, '#36393E'], ['Silicone rubber', 1.10, '#D6D8DC'],
];
const MAT_COLOR = Object.fromEntries(LIBRARY.map(([n, , c]) => [n, c]));
const DENSITY = Object.fromEntries(LIBRARY.map(([n, d]) => [n, d]));
// Auto part colours, as the app computes them (PartPalette.cpp), first 12 of each family.
const PALETTES = [
  ['Cool shades', ['#71ADD0', '#8572BB', '#11957C', '#74E2C6', '#CEC0FD', '#2D8AB8', '#A996E2', '#4ABBA1', '#79D9FE', '#6F9D96', '#94C2BB', '#7190D6']],
  ['Warm shades', ['#CEA175', '#BF696A', '#8A8C34', '#D5D881', '#FEBFBD', '#B37736', '#B58C8C', '#BFC19C', '#E88E8E', '#AFB15B', '#FAA684', '#B5943F']],
  ['Muted', ['#7FB6AB', '#BD917F', '#9FB1D8', '#9AAC83', '#D8AABF', '#80B7C3', '#B69A76', '#B8ADD7', '#87B39A', '#BD8C89', '#90B4D4', '#A6A67A']],
  ['High contrast', ['#26D3BB', '#C75C2C', '#95B6FE', '#679012', '#FE8DC5', '#198FA3', '#EFA831', '#8B69CD', '#4AD496', '#CA5555', '#74BFFD', '#898501']],
];

// Loupe's custom colour dialog: Apple's system colours and the classic Crayons palette
// (the same values as src/app/qml/inspect/ThemedColorDialog.qml).
const SYSTEM_COLORS = ['#FF3B30', '#FF9500', '#FFCC00', '#34C759', '#00C7BE', '#30B0C7', '#32ADE6', '#007AFF', '#5856D6', '#AF52DE', '#FF2D55', '#A2845E', '#8E8E93'];
const CRAYONS = [
  '#990000', '#7f462c', '#808000', '#4e9a06', '#339900', '#4a5d23', '#0f5c4b', '#0066cc', '#660066', '#dda0dd',
  '#ff2600', '#ff7f00', '#fffe00', '#40ff00', '#66ffb2', '#00ffcc', '#56b4e3', '#660099', '#cc0099', '#ff6699',
  '#ff8c69', '#ffcc66', '#ffff66', '#e6ffcc', '#66ff66', '#66ffcc', '#b5f4e0', '#c3a0de', '#8000ff', '#cc99ff',
  '#000000', '#333333', '#666666', '#808080', '#999999', '#b3b3b3', '#cccccc', '#dddddd', '#eeeeee', '#ffffff',
];

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
    color, metalness: 0, roughness: 0.6, emissive: '#000000', // as the app: rough, no metal, near-zero specular
    clippingPlanes: [clipWorld], transparent: true,
  });
  const mesh = new THREE.Mesh(geom, mat);
  const cap = new THREE.Mesh(geom, new THREE.MeshBasicMaterial({ color: CAP, side: THREE.BackSide, clippingPlanes: [clipWorld], transparent: true }));
  const edges = new THREE.LineSegments(
    new THREE.EdgesGeometry(geom, 28),
    new THREE.LineBasicMaterial({ color: '#EEF2FF', transparent: true, opacity: 1, clippingPlanes: [clipWorld] }),
  );
  group.add(cap, mesh, edges);
  group.position.copy(pos);
  model.add(group);
  const p = {
    def, group, mesh, cap, edges,
    home: pos.clone(), dir: explode.clone(),
    base: new THREE.Color(color), warm: new THREE.Color(opts.warm ?? color),
    order: opts.order ?? parts.length,
    halfX: (geom.computeBoundingBox(), (geom.boundingBox.max.x - geom.boundingBox.min.x) / 2),
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
    { warm: '#CEA175', material: 'Aluminum 6061', volume: vol.box(220, 8, 70), order: 0 });
  for (const s of [-1, 1]) addPart('End Block', new THREE.BoxGeometry(20, 50, 60), '#c3c7cf', V(100 * s, 33, 0), V(62 * s, 0, 0),
    { warm: '#BF696A', material: 'Aluminum casting', volume: vol.box(20, 50, 60), order: 1 });
  for (const s of [-1, 1]) addPart('Guide Rod', cylX(4, 184), '#d9dde4', V(0, 44, 20 * s), V(0, 8, 48 * s),
    { metal: 0.5, rough: 0.3, warm: '#8A8C34', material: 'Stainless steel 304', volume: vol.cyl(4, 184), order: 2 });
  addPart('Lead Screw', cylX(3.5, 232, 24), '#b9bec8', V(16, 30, 0), V(0, -24, 0),
    { metal: 0.5, rough: 0.35, warm: '#D5D881', material: 'Carbon steel', volume: vol.cyl(3.5, 232), order: 3 });
  addPart('Slider', new THREE.BoxGeometry(50, 24, 56), '#4048c8', V(0, 40, 0), V(0, 18, 0),
    { warm: '#B58C8C', material: 'Polyamide (PA / nylon)', volume: vol.box(50, 24, 56), order: 5 });
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) addPart('Bushing', cylX(6.5, 10, 24), '#c99a5f', V(30 * sx, 44, 20 * sz), V(34 * sx, 18, 0),
    { metal: 0.45, rough: 0.35, warm: '#BFC19C', material: 'Copper', volume: vol.cyl(6.5, 10), order: 6 });
  const plateGeom = new THREE.ExtrudeGeometry(plateShape(), { depth: PLATE.t, bevelEnabled: false, curveSegments: 24 }).rotateX(-Math.PI / 2);
  addPart('Top Plate', plateGeom, '#c4c8d0', V(0, 52, 0), V(0, 58, 0),
    { warm: '#E88E8E', material: 'Aluminum 6061', volume: plateArea * PLATE.t, order: 7 });
  for (const [x, z] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) addPart('M5 Screw', cylY(4, 3.5, 20), '#1b1c21', V(x * 25, 58.75, z * 25), V(0, 96, 0),
    { warm: '#AFB15B', order: 8 });
  for (const [x, z] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) addPart('M6 Screw', cylY(5, 2.5, 20), '#1b1c21', V(x * 70, 9.25, z * 27), V(0, -78, 0),
    { warm: '#FEBFBD', order: 4 });
  addPart('Motor', new THREE.BoxGeometry(42, 42, 42), '#24262c', V(133, 37, 0), V(128, 0, 0), { warm: '#B37736', order: 9 });
  addPart('Motor', new THREE.BoxGeometry(8, 40, 40), '#8d929c', V(158, 37, 0), V(128, 0, 0), { warm: '#B37736', order: 9 });
  defs.Motor.count = 1;
}
buildModel();

const plate = defs['Top Plate'].parts[0];
const trimmed = [...defs.Motor.parts, ...defs['M6 Screw'].parts];


// ---------------------------------------------------------------------------------------------
// Renderer
// ---------------------------------------------------------------------------------------------
let VW = viewportEl.clientWidth || 598, VH = viewportEl.clientHeight || 589;
let renderer = null;
const scene = new THREE.Scene();
scene.add(pivot);
scene.add(new THREE.AmbientLight('#ffffff', 1.25));
const fill = new THREE.DirectionalLight('#ffffff', 0.35); fill.position.set(-300, 300, -300); scene.add(fill);

const FRUSTUM = 250;
const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, -2000, 4000);
function setFrustum() {
  const a = VW / VH;
  camera.left = -FRUSTUM * a / 2; camera.right = FRUSTUM * a / 2;
  camera.top = FRUSTUM / 2; camera.bottom = -FRUSTUM / 2;
  camera.updateProjectionMatrix();
}
setFrustum();
const key = new THREE.DirectionalLight('#ffffff', 1.9); // rides on the camera, head-on, as in the app
key.position.set(0, 0, 1); camera.add(key); key.target.position.set(0, 0, -1); camera.add(key.target); scene.add(camera);
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
// Inspect: one row per occurrence, labelled "Name  4x" as in the app. Groups get a chevron.
$('#tree-inspect').innerHTML = TREE.flatMap(([n, d, c, k]) => k
  ? [`<li class="grp" data-def="${n}" style="padding-left:${4 + d * 12}px">${n}</li>`]
  : Array.from({ length: c }, (_, o) => `<li data-def="${n}" data-occ="${o}" style="padding-left:${18 + d * 12}px">${n}&nbsp;&nbsp;${c}x</li>`)).join('');
// Export ("Components", with checkboxes) and Drawing ("Parts") pickers: no quantities.
const pickerRows = check => TREE.map(([n, d, c, k]) => `<li class="${k ? 'grp' : ''}" data-def="${n}" style="padding-left:${4 + d * 12 + (k ? 0 : 14)}px">${check && !k ? '<i class="cb"></i>' : ''}${n}${!check && n === 'Top Plate' ? '<em class="qchip" id="q-chip">1</em>' : ''}</li>`).join('');
$('#tree-drawing').innerHTML = pickerRows(false);
const EXPORT_ORDER = ['Top Plate', 'Slider', 'End Block', 'Base Plate', 'Guide Rod', 'Lead Screw', 'Bushing', 'Motor'];
$('#tree-export').innerHTML = pickerRows(true);
$('#bucket').innerHTML = EXPORT_ORDER.map(n => `<li data-def="${n}"><b>${n}</b><span class="fn">${n}<i>.step</i></span><small>Ready</small></li>`).join('');
const LANE_COLORS = ['#8B93F8', '#4ABBA1', '#E8A35C', '#79D9FE', '#E88E8E', '#CEC0FD', '#D5D881', '#71ADD0'];
$('#pop-files').innerHTML = EXPORT_ORDER.map((n, i) => `<div class="fchip" style="--lane:${LANE_COLORS[i]}"><svg><use href="#i-file"/></svg><span>${n}.step</span><small>written</small></div>`).join('');

const MASS_STEPS = ['Base Plate', 'End Block', 'Guide Rod', 'Lead Screw', 'Slider', 'Bushing', 'Top Plate'];
const massStep = () => S.mass <= 0 ? -1 : Math.min(MASS_STEPS.length, Math.floor(seg(S.mass, 0.06, 0.8) * (MASS_STEPS.length + 1)));
const assignedK = def => { const i = MASS_STEPS.indexOf(def); return i < 0 || S.mass <= 0 ? 0 : clamp(seg(S.mass, 0.06, 0.8) * (MASS_STEPS.length + 1) - i); };
const gramsOf = d => d.volume / 1000 * DENSITY[d.material];
const areaOf = geom => {
  const g = geom.index ? geom.toNonIndexed() : geom, p = g.attributes.position, A = new THREE.Vector3(), B = new THREE.Vector3(), C = new THREE.Vector3();
  let sum = 0;
  for (let i = 0; i < p.count; i += 3) { A.fromBufferAttribute(p, i); B.fromBufferAttribute(p, i + 1); C.fromBufferAttribute(p, i + 2); sum += B.sub(A).cross(C.sub(A)).length() / 2; }
  return sum;
};
Object.values(defs).forEach(d => { d.area = areaOf(d.parts[0].mesh.geometry); });
const fmtArea = v => `${v.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })} mm²`;
const MASS_TOTAL = MASS_STEPS.reduce((a, n) => a + gramsOf(defs[n]) * defs[n].count, 0);
const VOL_TOTAL = MASS_STEPS.reduce((a, n) => a + defs[n].volume * defs[n].count, 0);
const fmtMass = g => g < 1000 ? `${g.toFixed(2)} g` : `${(g / 1000).toFixed(2)} kg`;
const fmtVol = v => `${v.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })} mm³`;
$('#mf-list').innerHTML = LIBRARY.map(([n, d, c]) => `<li><i style="background:${c}"></i><span>${n}</span><em>${d.toFixed(2)} g/cm³</em></li>`).join('');
$('#pick').innerHTML = `<div class="pal"><span>System colours</span><div class="pick-sys">${SYSTEM_COLORS.map(c => `<i style="background:${c}"></i>`).join('')}</div></div>`
  + `<div class="pal"><span>Crayons</span>${[0, 1, 2, 3].map(r => `<div class="pick-cr${r % 2 ? ' alt' : ''}">${CRAYONS.slice(r * 10, r * 10 + 10).map(c => `<i style="background:${c}"></i>`).join('')}</div>`).join('')}</div>`;
$('#pal').innerHTML = PALETTES.map(([n, cs]) => `<div class="pal"><span>${n}</span><div>${cs.map(c => `<i style="background:${c}"></i>`).join('')}</div></div>`).join('');
// Pop cards with two scenarios (open race, export race): the arrows flip between them, and the
// new scenario's bars grow in once it is showing.
$$('.mini').forEach(m => {
  const slides = $$('.ms', m), dots = $$('.mini-dots i', m);
  let i = 0;
  const show = k => {
    i = (k + slides.length) % slides.length;
    slides.forEach((s, j) => { s.classList.toggle('on', j === i); s.classList.toggle('grow', false); });
    void slides[i].offsetWidth; // restart the grow animation
    slides[i].classList.add('grow');
    dots.forEach((d, j) => d.classList.toggle('on', j === i));
  };
  $('.mini-prev', m).addEventListener('click', () => show(i - 1));
  $('.mini-next', m).addEventListener('click', () => show(i + 1));
});

// Measured section: one scenario card at a time. Tabs, arrows, swipe and the arrow keys all move
// the same scroll-snap track; the scroll position is the only state, so they can't disagree.
// A card's bars grow the first time it is shown while the section is on screen.
(() => {
  const sc = $('#bc');
  if (!sc) return;
  const track = $('#bc-track', sc), slides = [...track.children], tabs = $$('[role=tab]', sc);
  const prev = $('.bc-prev', sc), next = $('.bc-next', sc);
  let cur = -1, seen = false;
  const step = () => slides[1].offsetLeft - slides[0].offsetLeft;
  const go = i => {
    i = clamp(i, 0, slides.length - 1);
    track.scrollTo({ left: i * step(), behavior: reduceMotion ? 'auto' : 'smooth' });
  };
  const set = i => {
    if (i === cur) return;
    cur = i;
    tabs.forEach((t, k) => { t.setAttribute('aria-selected', k === i); t.tabIndex = k === i ? 0 : -1; });
    slides.forEach((s, k) => s.setAttribute('aria-hidden', k !== i));
    prev.disabled = i === 0;
    next.disabled = i === slides.length - 1;
    if (seen) slides[i].classList.add('in');
  };
  track.addEventListener('scroll', () => set(clamp(Math.round(track.scrollLeft / step()), 0, slides.length - 1)), { passive: true });
  tabs.forEach((t, k) => t.addEventListener('click', () => go(k)));
  prev.addEventListener('click', () => go(cur - 1));
  next.addEventListener('click', () => go(cur + 1));
  sc.addEventListener('keydown', e => {
    const d = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
    if (!d) return;
    e.preventDefault();
    const i = clamp(cur + d, 0, slides.length - 1);
    go(i);
    if (tabs.includes(document.activeElement)) tabs[i].focus({ preventScroll: true });
  });
  set(0);
  const io = new IntersectionObserver(es => {
    if (!es.some(en => en.isIntersecting)) return;
    seen = true;
    slides[cur].classList.add('in');
    io.disconnect();
  }, { threshold: 0.4 });
  io.observe(track);
})();

// 2D slice at X = 0 through the carriage, in the YZ plane, drawn from the same dimensions.
// The cut at X = 0, from the same dimensions as the parts. The viewer's 2D slice and the figure
// beside it are both drawn from this list: [part, rect z0 z1 y0 y1 | circle z y r].
const SECTION = [
  ['Base Plate', 'r', -35, 35, 0, 8], ['Slider', 'r', -28, 28, 28, 52],
  ['Guide Rod', 'c', -20, 44, 4], ['Guide Rod', 'c', 20, 44, 4], ['Lead Screw', 'c', 0, 30, 3.5],
  ['Top Plate', 'r', -35, -9, 52, 57], ['Top Plate', 'r', 9, 35, 52, 57],
];
(function drawSlice() {
  const Y = y => 60 - y, col = d => '#' + defs[d].parts[0].warm.getHexString();
  $('#slice-svg').innerHTML = SECTION.map(([d, k, a, b, c, e]) => k === 'r'
    ? `<rect x="${a}" y="${Y(e)}" width="${b - a}" height="${e - c}" fill="${col(d)}" stroke="#EEF2FF"/>`
    : `<circle cx="${a}" cy="${Y(b)}" r="${c}" fill="${col(d)}" stroke="#EEF2FF"/>`).join('');
})();
const sliceGroup = new THREE.Group();
sliceGroup.rotation.y = Math.PI / 2; // shape (u, v) -> (-z, y): seen from +x, as "2D slice only" looks at it
SECTION.forEach(([d, k, a, b, c, e], i) => {
  const shape = new THREE.Shape();
  if (k === 'r') { shape.moveTo(-a, c); shape.lineTo(-b, c); shape.lineTo(-b, e); shape.lineTo(-a, e); shape.closePath(); }
  else shape.absarc(-a, b, c, 0, Math.PI * 2, false);
  const fill = new THREE.Mesh(new THREE.ShapeGeometry(shape, 32), new THREE.MeshBasicMaterial({ color: defs[d].parts[0].warm, transparent: true, side: THREE.DoubleSide, depthTest: false }));
  const line = new THREE.LineLoop(new THREE.BufferGeometry().setFromPoints(shape.getPoints(48)), new THREE.LineBasicMaterial({ color: '#EEF2FF', transparent: true, depthTest: false }));
  fill.renderOrder = 10 + i; line.renderOrder = 30 + i;
  sliceGroup.add(fill, line);
});
sliceGroup.visible = false;
model.add(sliceGroup);

// Plate outline for the Drawing workspace: the 2D preview and the big pop share one path.
const plateOutline = (() => {
  const { w, r, bore, hole, pitch } = PLATE, h = w / 2;
  const outer = `M${-h + r} ${-h}H${h - r}A${r} ${r} 0 0 1 ${h} ${-h + r}V${h - r}A${r} ${r} 0 0 1 ${h - r} ${h}H${-h + r}A${r} ${r} 0 0 1 ${-h} ${h - r}V${-h + r}A${r} ${r} 0 0 1 ${-h + r} ${-h}Z`;
  const circle = (x, y, rr) => `M${x - rr} ${y}a${rr} ${rr} 0 1 0 ${rr * 2} 0a${rr} ${rr} 0 1 0 ${-rr * 2} 0Z`;
  const holes = [circle(0, 0, bore), ...[[-1, -1], [1, -1], [1, 1], [-1, 1]].map(([x, y]) => circle(x * pitch, y * pitch, hole))];
  return [outer, ...holes];
})();
$('#pv2d').innerHTML = plateOutline.map(d => `<path d="${d}" pathLength="1"/>`).join('');
$('#outline-svg').innerHTML = `
  <defs><radialGradient id="hot"><stop offset="0" stop-color="#fff6e0"/><stop offset="0.25" stop-color="#ffb347" stop-opacity="0.9"/><stop offset="1" stop-color="#ff5a1f" stop-opacity="0"/></radialGradient></defs>
  <rect class="sheet" x="-46" y="-46" width="92" height="92" rx="1.2"/>
  <path class="pocket" d="${plateOutline.join(' ')}" fill-rule="evenodd"/>
  <g class="contours">${plateOutline.map(d => `<path d="${d}" pathLength="1"/>`).join('')}</g>
  <g class="kerf">${plateOutline.map(d => `<path d="${d}"/>`).join('')}</g>
  <path class="part" id="cut-part" d="${plateOutline.join(' ')}" fill-rule="evenodd"/>
  <g class="head" id="cut-head"><circle r="4.5" fill="url(#hot)"/><circle r="0.8" class="core"/></g>`;

// ---------------------------------------------------------------------------------------------
// State, and how it is drawn
// ---------------------------------------------------------------------------------------------
const S = {
  k: 0, heroOut: 0, spin: 1, rot: 0.35,
  open: 0, race: 0,
  warm: 0, ghost: 0, capture: 0, layers: 0, compose: 0,
  exWin: 0, explode: 0,
  sec: 0, secPos: 160, slice: 0,
  measure: 0, mass: 0,
  ws: 0, split: 0, files: 0, trim: 0, onefile: 0, menu: 0, trimx: 0,
  top: 0, iso: 0, outline: 0, cut: 0, lift: 0, bench: 0, benchOut: 0,
  tools: '',
};

const tmpC = new THREE.Color(), tmpV = new THREE.Vector3(), tmpW = new THREE.Vector3();
const ACCENT = new THREE.Color('#8B93F8');
const AMBER = new THREE.Color('#FFD166'), AMBER_EMIT = new THREE.Color('#3A2C00');
const EDGE = new THREE.Color('#EEF2FF'), EDGE_SEL = new THREE.Color('#FFF0A6');
let soloDef = null, prevSec = 0, prevEx = 0, curSplitDef = null;
const CONTEXT_GREY = new THREE.Color('#aab1c2');
const CARRIAGE = new Set(['Slider', 'Bushing', 'Top Plate', 'M5 Screw']);
let renderMode = null; // null | 'context' (everything ghosted) | 'focus' (carriage only)
let t0 = performance.now(), lastNow = t0, spinAngle = 0;

// Drag to orbit. The offset sits on top of the scripted pose; scrolling eases it back to zero.
const orbit = { yaw: 0, pitch: 0, vYaw: 0, vPitch: 0, dragging: false, homing: false, touched: false, x: 0, y: 0, lt: 0 };
const PITCH_MIN = -0.55, PITCH_MAX = 0.95; // camera elevation offset, radians
const UPV = new THREE.Vector3(0, 1, 0), tmpR = new THREE.Vector3();
function stepOrbit(dt) {
  if (orbit.dragging) return;
  if (orbit.homing) {
    const k = 1 - Math.pow(0.0015, dt); // about 90% of the way back in a third of a second
    orbit.yaw -= orbit.yaw * k; orbit.pitch -= orbit.pitch * k;
    orbit.vYaw = orbit.vPitch = 0;
    if (Math.abs(orbit.yaw) < 1e-3 && Math.abs(orbit.pitch) < 1e-3) { orbit.yaw = orbit.pitch = 0; orbit.homing = false; }
    return;
  }
  const decay = Math.pow(0.03, dt); // inertia after a flick
  orbit.yaw += orbit.vYaw * dt; orbit.vYaw *= decay;
  orbit.pitch = Math.min(PITCH_MAX, Math.max(PITCH_MIN, orbit.pitch + orbit.vPitch * dt)); orbit.vPitch *= decay;
}
function homeOrbit() {
  if (orbit.dragging || (!orbit.yaw && !orbit.pitch && !orbit.vYaw)) return;
  orbit.yaw = Math.atan2(Math.sin(orbit.yaw), Math.cos(orbit.yaw)); // unwind whole turns first
  orbit.homing = true;
}

function apply(now = performance.now()) {
  const t = (now - t0) / 1000;
  const dt = Math.min(0.05, Math.max(0, (now - lastNow) / 1000)); lastNow = now;

  // --- model pose ---
  // The hero spin. Kept within one turn (invisible while it spins) and frozen once the hand-off to the
  // story starts, so blending it out unwinds at most half a turn instead of every turn since load.
  if (!orbit.dragging && S.spin > 0.999) { spinAngle += dt * 0.3; spinAngle = Math.atan2(Math.sin(spinAngle), Math.cos(spinAngle)); }
  stepOrbit(dt);
  const idle = S.spin * spinAngle + (1 - S.spin) * Math.sin(t * 0.45) * 0.14 * (1 - S.top);
  pivot.rotation.y = S.rot + idle + orbit.yaw;

  const isoK = Math.max(S.iso, clamp((S.ws - 1.5) * 2)); // the Drawing workspace shows only the chosen part
  const visibleCount = S.open <= 0 ? parts.length : Math.floor(parts.length * seg(S.open, 0.08, 0.92));
  const byOrder = [...parts].sort((a, b) => a.order - b.order);
  const shown = new Set(byOrder.slice(0, visibleCount));
  const splitIdx = Math.floor(S.split * (EXPORT_ORDER.length + 0.999)) - 1; // part being ticked
  const splitDef = splitIdx >= 0 && splitIdx < EXPORT_ORDER.length && S.split < 1 ? EXPORT_ORDER[splitIdx] : null;
  const ms = massStep(), sliceK = S.slice * S.sec;
  curSplitDef = splitDef;
  // the app hides edges while the section plane is dragged or parts are moving
  const moving = Math.abs(S.secPos - prevSec) > 1e-3 && S.sec > 0.01 || Math.abs(S.explode - prevEx) > 1e-4;
  prevSec = S.secPos; prevEx = S.explode;

  parts.forEach((p, i) => {
    // explode, trim drop
    const isTrim = trimmed.includes(p);
    const drop = isTrim ? S.trim : 0;
    p.group.position.copy(p.home).addScaledVector(p.dir, S.explode);

    // colour: base -> warm (present) -> material (mass)
    tmpC.copy(p.base).lerp(p.warm, S.warm);
    const d = defs[p.def];
    if (d.material) tmpC.lerp(new THREE.Color(MAT_COLOR[d.material]), assignedK(p.def));
    // selection is amber in the app; in Export the component being previewed is highlighted the same way
    const sel = renderMode ? 0 : (ms >= 0 && (ms >= MASS_STEPS.length || p === defs[MASS_STEPS[ms]].parts[0])) || p.def === splitDef ? 1 : 0;
    if (renderMode === 'context') tmpC.lerp(CONTEXT_GREY, 0.6);
    const capC = tmpC.clone();
    tmpC.lerp(AMBER, sel);
    p.mesh.material.color.copy(tmpC);
    p.mesh.material.emissive.copy(AMBER_EMIT).multiplyScalar(sel);
    p.edges.material.color.copy(EDGE).lerp(EDGE_SEL, sel);
    p.cap.material.color.copy(capC).multiplyScalar(1.08); // caps: the part's own colour, flat

    // opacity: ghosted housings (present), isolate plate (drawing), trimmed parts
    let a = 1;
    if (p.def === 'End Block' || p.def === 'Motor') a = Math.min(a, 1 - S.ghost * 0.55); // transparent = 45%
    if (p !== plate) a = Math.min(a, 1 - isoK);
    a = Math.min(a, 1 - drop);
    const appear = shown.has(p) ? 1 : 0;
    a *= appear;
    if (renderMode === 'context') a = Math.min(a, 0.3);
    if (renderMode === 'focus') a = CARRIAGE.has(p.def) ? appear : 0;
    if (renderMode === 'solo') a = p.def === soloDef ? 1 : 0;
    if (renderMode === 'kept') a = isTrim ? 0 : 1;
    p.group.visible = a > 0.01;
    const solid = a * (1 - sliceK); // "2D slice only" hides every solid and edge
    p.mesh.material.opacity = solid;
    p.mesh.material.depthWrite = solid > 0.98;
    p.cap.material.opacity = a * (1 - sliceK); // in 2D slice the exact section below takes over
    p.cap.visible = S.sec > 0.01;
    p.edges.material.opacity = moving ? 0 : solid;
  });

  // section plane follows the model as it turns
  clipLocal.constant = S.sec > 0.01 ? S.secPos : 1e4;
  pivot.updateMatrixWorld(true);
  clipWorld.copy(clipLocal).applyMatrix4(model.matrixWorld);

  // camera: iso -> top view of the plate for the Drawing workspace
  tmpV.copy(ISO).lerp(TOP, S.top).normalize();
  if (sliceK > 0) tmpV.lerp(V(1, 0.0001, 0).applyAxisAngle(UPV, pivot.rotation.y), sliceK).normalize();
  if (orbit.pitch) { // drag up or down: raise or lower the camera, like a CAD orbit
    tmpR.crossVectors(UPV, tmpV);
    if (tmpR.lengthSq() > 1e-6) tmpV.applyAxisAngle(tmpR.normalize(), -orbit.pitch);
  }
  tmpW.set(0, 32, 0);
  if (S.sec > 0) tmpW.lerp(new THREE.Vector3(-58, 34, 0).applyMatrix4(model.matrixWorld), S.sec); // frame the kept half
  if (S.top > 0 || isoK > 0) {
    const plateWorld = plate.group.getWorldPosition(new THREE.Vector3()).add(V(0, 2.5, 0));
    tmpW.lerp(plateWorld, Math.max(S.top, isoK));
  }
  if (sliceK > 0) tmpW.lerp(V(S.secPos, 30, 0).applyMatrix4(model.matrixWorld), sliceK);
  sliceGroup.visible = sliceK > 0.01;
  sliceGroup.position.x = S.secPos + 0.1;
  sliceGroup.children.forEach(m => { m.material.opacity = sliceK; });
  const zoomNow = lerp(1, 2, isoK) * lerp(1, 1.275, S.top) * lerp(1, 0.78, S.explode) * lerp(1, 2.4, sliceK);
  const winK = Math.max(S.capture, S.exWin, S.sec, seg(S.measure, 0, 0.2)) * clamp(1 - S.ws * 2);
  if (winK > 0.001) { // a floating tool window covers the right of the view: keep the model in the clear part
    const twW = (S.exWin > 0.5 || S.measure > 0.01 ? 340 : 320) + 12;
    tmpR.copy(tmpV).negate().cross(UPV).normalize();
    tmpW.addScaledVector(tmpR, (twW / 2) * FRUSTUM / (VH * zoomNow) * winK);
  }
  camera.position.copy(tmpW).addScaledVector(tmpV, 800);
  camera.up.set(0, 1 - S.top, -S.top).normalize();
  camera.lookAt(tmpW);
  camera.zoom = zoomNow; // explode: the app doesn't refit; the scene shows Fit pressed
  camera.updateProjectionMatrix();

  if (renderer && renderActive) { renderer.render(scene, camera); viewCube?.draw(tmpV); }

  drawOverlay();
  syncChrome();
  xhandle?.frame(now, renderActive && S.exWin > 0.01 && !renderMode);
}

// Measurement dimensions, projected from model points into the viewport each frame.
const MEAS = {
  rad: { c: V(0, 57, 0), p: V(9, 57, 0), text: 'R 9.000 · Ø 18.000 mm', pinned: 'Radius  R 9.000 · Ø 18.000 mm' },
  dist: { a: V(-62, 44, -20), b: V(-62, 44, 20), text: '40.000 mm' },
};
function project(v) {
  tmpV.copy(v).applyMatrix4(model.matrixWorld).project(camera);
  return [(tmpV.x + 1) / 2 * VW, (1 - tmpV.y) / 2 * VH];
}
const AMB = '#FFD166', HALO = '#FFF4CF';
const tagSvg = (x, y, text, op = 1) => { const w = text.length * 6.5 + 16; return `<g opacity="${op}"><rect x="${x - w / 2}" y="${y - 10}" width="${w}" height="20" rx="6" fill="${AMB}" stroke="${HALO}"/><text x="${x}" y="${y + 4.2}" text-anchor="middle">${text}</text></g>`; };
const lineSvg = (x1, y1, x2, y2) => `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${HALO}" stroke-opacity="0.55" stroke-width="3.5"/><line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${AMB}" stroke-width="1.5"/>`;
const pickSvg = (x, y, n, pinned) => pinned ? `<circle cx="${x}" cy="${y}" r="4" fill="${AMB}" stroke="${HALO}"/>`
  : `<circle cx="${x}" cy="${y}" r="10" fill="${AMB}" stroke="${HALO}" stroke-width="2"/><text x="${x}" y="${y + 4}" text-anchor="middle" class="pk">${n}</text>`;
function drawOverlay() {
  let out = '';
  overlay.setAttribute('viewBox', `0 0 ${VW} ${VH}`);
  // section handle: a ball on the plane and an arrow pointing at the removed side
  const hk = seg(S.sec, 0.3, 0.6) * (1 - S.slice);
  if (hk > 0.01) {
    const [cx, cy] = project(V(S.secPos, 34, 0)), [tx, ty] = project(V(S.secPos + 30, 34, 0));
    const L = Math.hypot(tx - cx, ty - cy) || 1, ux = (tx - cx) / L, uy = (ty - cy) / L, ex = cx + ux * 70, ey = cy + uy * 70;
    out += `<g opacity="${hk}"><line x1="${cx}" y1="${cy}" x2="${ex}" y2="${ey}" stroke="#A78BFA" stroke-width="3"/>`
      + `<path d="M${ex + ux * 16} ${ey + uy * 16}L${ex - uy * 7} ${ey + ux * 7}L${ex + uy * 7} ${ey - ux * 7}Z" fill="#A78BFA"/>`
      + `<circle cx="${cx}" cy="${cy}" r="7" fill="#F5F3FF"/></g>`;
  }
  if (S.measure > 0.01) {
    // radius: a leader from the circle's centre to the picked point, with a cross at the centre
    const rk = seg(S.measure, 0.12, 0.4), pinned = S.measure >= 0.5;
    if (rk > 0) {
      const [cx, cy] = project(MEAS.rad.c), [px, py] = project(MEAS.rad.p);
      out += lineSvg(cx, cy, lerp(cx, px, rk), lerp(cy, py, rk)) + `<path d="M${cx - 5} ${cy}h10M${cx} ${cy - 5}v10" stroke="${AMB}" stroke-width="1.5"/>` + pickSvg(px, py, 1, pinned);
      if (rk > 0.95) out += tagSvg((cx + px) / 2, (cy + py) / 2, pinned ? MEAS.rad.pinned : MEAS.rad.text, seg(rk, 0.95, 1));
    }
    // distance: a line with square end ticks and the value centred on it
    const dk = seg(S.measure, 0.55, 0.85);
    if (dk > 0) {
      const [ax, ay] = project(MEAS.dist.a), [bx, by] = project(MEAS.dist.b);
      const L = Math.hypot(bx - ax, by - ay) || 1, nx = -(by - ay) / L * 6, ny = (bx - ax) / L * 6;
      const ex = lerp(ax, bx, dk), ey = lerp(ay, by, dk);
      out += lineSvg(ax, ay, ex, ey) + `<path d="M${ax - nx} ${ay - ny}L${ax + nx} ${ay + ny}" stroke="${AMB}" stroke-width="1.5"/>` + pickSvg(ax, ay, 1, false);
      if (dk > 0.98) out += `<path d="M${bx - nx} ${by - ny}L${bx + nx} ${by + ny}" stroke="${AMB}" stroke-width="1.5"/>` + pickSvg(bx, by, 2, false) + tagSvg((ax + bx) / 2, (ay + by) / 2, MEAS.dist.text);
    }
  }
  overlay.innerHTML = out;
}

// Chrome: workspace tabs, toolbar, tool windows, panels and page-scale callouts.
const toolEls = Object.fromEntries($$('#toolbar [data-tool]').map(e => [e.dataset.tool, e]));
const tabEls = $$('#ws-seg [data-tab]');
const railCards = $$('#rail .card');
const wsRows = $$('.ws-list li');
const treeExport = $$('#tree-export li');
const treeInspect = $$('#tree-inspect li');
const bucketRows = $$('#bucket li');
const fileChips = $$('#pop-files .fchip');
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
    navWs.forEach((e, i) => e.classList.toggle('on', i === wsIdx));
    wsRows.forEach((e, i) => e.classList.toggle('on', i === wsIdx));
    app.dataset.ws = ['inspect', 'export', 'drawing'][wsIdx];
    $('#vp-label').textContent = ['', 'Master assembly', 'Click a flat face, or a cube face'][wsIdx];
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
  // loading, as in the app: centred until the first parts are drawn, then a card at the top
  const pct = Math.round(seg(S.open, 0, 0.95) * 100), firstParts = seg(S.open, 0.08, 0.92) > 0;
  vis($('#vp-load'), opening ? 1 : 0);
  $('#vp-load').classList.toggle('docked', firstParts);
  $('#vl-fill').style.width = `${pct}%`;
  $('#vl-meta').textContent = `${pct}%  •  0 s  •  ${pct > 80 ? 'almost done' : 'estimating remaining time'}`;
  $('#vl-btn').textContent = firstParts ? 'Cancel refinement' : 'Cancel import';
  if (ws > 0.5 && ws < 1.5) $('#vp-label').textContent = curSplitDef ? `Master assembly · ${curSplitDef} highlighted` : 'Master assembly';
  vis($('#dd-proj'), 1 - clamp(ws * 2)); // the projection menu is Inspect-only
  $('#pc-mode').textContent = S.warm > 0.5 ? 'Auto · warm shades' : 'File colours';
  vis($('#pc-recolour'), S.warm > 0.5 ? 1 : 0);
  $$('#props-doc [data-count]').forEach(e => {
    const target = +e.dataset.count;
    const k = S.open <= 0 ? 1 : seg(S.open, 0.1, 0.4 + target / 1400);
    e.textContent = k <= 0 ? '—' : `${Math.round(target * k)} ms`;
  });

  // toolbar highlights
  const on = new Set();
  if (S.ghost > 0.3) on.add('transparent');
  if (S.capture > 0.3) on.add('capture');
  if (S.explode > 0.3 && S.exWin > 0.5) on.add('fit'); // explode doesn't refit in the app; Fit does
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
  $('#ex-fill').style.width = `${S.explode / 3 * 100}%`; // the slider runs 0-3; the first spread is 1
  $('#sec-pos').textContent = Math.round(S.secPos);
  $('#sw-slice').classList.toggle('on', S.slice > 0.5);
  $('#tw-section').classList.toggle('slice-on', S.slice > 0.5);
  const mStep = S.measure < 0.5 ? '1' : '2';
  if ($('#tw-measure').dataset.step !== mStep) {
    $('#tw-measure').dataset.step = mStep;
    $$('#tw-measure .mm-grid span').forEach(e => e.classList.toggle('on', e.dataset.m === (mStep === '1' ? 'rad' : 'pts')));
  }

  // right panel: document props <-> materials and mass
  vis($('#props-doc'), 1 - clamp(S.mass * 4));
  vis($('#props-mass'), clamp(S.mass * 4));
  drawMass();

  // export: ticks and bucket
  const ticked = Math.floor(S.split * (EXPORT_ORDER.length + 0.999));
  const tickedSet = new Set(EXPORT_ORDER.slice(0, ticked));
  treeExport.forEach(e => {
    e.classList.toggle('on', tickedSet.has(e.dataset.def));
    e.classList.toggle('gone', S.trim > 0.4 && (e.dataset.def === 'Motor' || e.dataset.def === 'M6 Screw'));
  });
  const ms = massStep();
  treeInspect.forEach(e => e.classList.toggle('hid', S.trim > 0.5 && (e.dataset.def === 'Motor' || e.dataset.def === 'M6 Screw')));
  treeInspect.forEach(e => e.classList.toggle('sel', ms >= 0 && (ms >= MASS_STEPS.length ? !e.classList.contains('grp') : e.dataset.def === MASS_STEPS[ms] && e.dataset.occ === '0')));
  $$('#tree-drawing li').forEach(e => e.classList.toggle('sel', e.dataset.def === 'Top Plate' && S.ws > 1.5));
  bucketRows.forEach((e, i) => { e.style.display = i < ticked ? 'grid' : 'none'; });
  $('#comp-n').textContent = `${ticked} file${ticked === 1 ? '' : 's'}`;
  treeExport.forEach(e => e.classList.toggle('sel', e.dataset.def === curSplitDef));
  if (S.ws > 0.5 && !solosTaken) takeSolos();
  const spDef = curSplitDef || (ticked ? EXPORT_ORDER[Math.min(ticked, EXPORT_ORDER.length) - 1] : null);
  if (last.sp !== spDef) {
    last.sp = spDef;
    $('#sp-name').textContent = spDef ? `· ${spDef}` : '';
    $('#sp-img').src = spDef && soloShots[spDef] ? soloShots[spDef] : '';
    $('#sp').classList.toggle('has', !!(spDef && soloShots[spDef]));
  }
  const queued = S.outline > 0.5 ? 1 : 0;
  $('#q-n').textContent = `${queued} queued`;
  vis($('#q-chip'), queued); vis($('#q-badge'), queued);
  $('#bucket-n').textContent = `${ticked} file${ticked === 1 ? '' : 's'}`;
  $('#export-btn').textContent = ticked === 1 ? 'Export 1 file' : `Export ${ticked} files`;

  // drawing preview
  const draw = seg(S.ws, 1.5, 2); // the preview is there as soon as the workspace is
  pvPaths.forEach(p => { p.style.opacity = draw; });
  drawCut();

  // pops
  vis($('#pop-race'), S.race); $('#pop-race').style.setProperty('--race', seg(S.race, 0.2, 1));
  drawLayers();
  vis($('#pop-explode'), S.exWin); $('#pop-explode').style.transform = `translateY(${(1 - S.exWin) * 16}px)`;
  drawMeasureModes();
  vis($('#pop-slice'), S.slice); $('#pop-slice').style.transform = `translateY(${(1 - S.slice) * 24}px)`;
  drawSplit();
  drawTrim();
  vis($('#pop-outline'), seg(S.outline, 0, 0.1));

  drawHero();
}

// Hero copy fades with the scroll. Also called from the timeline's onUpdate, so it follows the
// scroll even when the render loop is not running (tab in the background, or a slow frame).
function drawHero() {
  if (!motion) return;
  const ho = $('#hero-copy');
  ho.style.opacity = 1 - S.heroOut; ho.style.visibility = S.heroOut > 0.99 ? 'hidden' : 'visible';
  ho.style.transform = `translate(-50%, ${-S.heroOut * 70}px)`;
}

// Mass: the right panel walks through the parts as each gets a material, then reads the whole
// assembly. Beside it, the built-in material library rolls past and the auto palettes fill in.
const mfWin = $('.mf-win'), mfRows = $$('#mf-list li'), palSw = $$('#pal i'), pickSw = $$('#pick i');
let mfUser = 0;
const mfBase = () => lerp(1, 7, seg(S.mass, 0.04, 0.9));
(function bindMaterialWheel() {
  const win = $('.mf-win'), maxC = LIBRARY.length - 1;
  const nudge = d => { // returns false at either end, so the page can keep scrolling
    const before = mfUser;
    mfUser = Math.min(maxC - mfBase(), Math.max(-mfBase(), mfUser + d));
    return mfUser !== before;
  };
  win.addEventListener('wheel', e => { if (S.mass > 0.1 && nudge(e.deltaY / 60)) e.preventDefault(); }, { passive: false });
  let dragY = null, moved = 0;
  win.addEventListener('pointerdown', e => { dragY = e.clientY; moved = 0; win.setPointerCapture(e.pointerId); });
  win.addEventListener('pointermove', e => { if (dragY === null) return; moved += Math.abs(e.clientY - dragY); nudge((dragY - e.clientY) / 26); dragY = e.clientY; });
  const stop = () => { dragY = null; };
  win.addEventListener('pointerup', stop); win.addEventListener('pointercancel', stop);
  mfRows.forEach((li, i) => li.addEventListener('click', () => { if (moved < 4) mfUser = i - mfBase(); }));
})();
function drawMass() {
  const ms = massStep();
  const title = $('#props-title');
  if (ms < 0) { title.textContent = 'Document properties'; }
  else if (ms < MASS_STEPS.length) {
    const d = defs[MASS_STEPS[ms]];
    title.textContent = 'Component properties';
    $('#sel-area').textContent = `Surface area  ${fmtArea(d.area)}`;
    $('#sel-vol').textContent = `Volume  ${fmtVol(d.volume)}`;
    $('#sel-mat').textContent = d.material;
    $('#sel-sw').style.background = MAT_COLOR[d.material];
    $('#sel-app').style.background = MAT_COLOR[d.material];
    $('#mass-total').textContent = `Estimated mass  ${fmtMass(gramsOf(d))}`;
  } else {
    title.innerHTML = `Selection properties <small>${MASS_STEPS.reduce((a, n) => a + defs[n].count, 0)} bodies</small>`;
    $('#sel-area').textContent = `Surface area  ${fmtArea(MASS_STEPS.reduce((a, n) => a + defs[n].area * defs[n].count, 0))}`;
    $('#sel-vol').textContent = `Volume  ${fmtVol(VOL_TOTAL)}`;
    $('#mass-total').textContent = `Estimated mass  ${fmtMass(MASS_TOTAL)}`;
  }
  vis($('#pop-mass'), seg(S.mass, 0.03, 0.18));
  if (S.mass <= 0) { mfUser = 0; return; }
  const c = Math.min(LIBRARY.length - 1, Math.max(0, mfBase() + mfUser)); // rests mid-list
  // Rows the window shows either side of the middle one: the wheel's curve and fade fit its height.
  const half = Math.max(3, mfWin.clientHeight / 2 / 26), tilt = Math.min(12, 50 / half);
  mfRows.forEach((e, i) => {
    const d = i - c, a = Math.abs(d);
    e.style.transform = `translateY(${d * 26}px) rotateX(${-d * tilt}deg) scale(${1 - Math.min(a, half) * (0.14 / half)})`;
    e.style.opacity = Math.max(0, 1 - a / (half + 0.4));
    e.classList.toggle('on', a < 0.5);
  });
  // the auto palettes fill in first, then the custom picker's swatches
  palSw.forEach((e, j) => { const k = seg(S.mass, 0.16 + j * 0.008, 0.28 + j * 0.008); e.style.transform = `scale(${k})`; e.style.opacity = k; });
  pickSw.forEach((e, j) => { const k = seg(S.mass, 0.5 + j * 0.0065, 0.62 + j * 0.0065); e.style.transform = `scale(${k})`; e.style.opacity = k; });
}

// Split: each ticked part leaves the master assembly on its own track. The tracks run parallel,
// bend together like lines on a metro map, and end at the file each part becomes.
const splitSvg = $('#split-lines');
// Export benchmark (measured, whole job): the three lanes start together and each stops at its own
// time, on one shared clock, so the bar lengths are the real times. Time and clicks show as it lands.
const benchLanes = $$('#pop-bench .ms:first-child .lane').map(e => ({ e, bar: e.querySelector('.track i'), s: +e.dataset.s }));
const BENCH_MAX = Math.max(...benchLanes.map(l => l.s));
let chipStep = 32, laneRank = null;
function drawSplit() {
  const on = S.files > 0;
  vis($('#pop-files'), on ? 1 : 0);
  vis(splitSvg, on ? 1 : 0);
  vis($('#pop-bench'), seg(S.bench, 0, 0.12) * (1 - S.benchOut));
  if (S.bench > 0 && S.benchOut < 1) {
    const t = seg(S.bench, 0.08, 1) * BENCH_MAX;
    benchLanes.forEach(l => {
      l.bar.style.setProperty('--f', (Math.min(t, l.s) / BENCH_MAX).toFixed(4));
      l.e.style.setProperty('--done', t >= l.s ? 1 : 0);
    });
  }
  if (!on) { laneRank = null; return; }
  if (stacked()) { // the app sits above the text, so the files simply fill in below
    splitSvg.innerHTML = '';
    const colW = $('#pop-files').offsetWidth / 2;
    fileChips.forEach((e, i) => {
      const k = seg(S.files, i / fileChips.length * 0.6, i / fileChips.length * 0.6 + 0.4);
      e.style.transform = `translate(${(i % 2) * colW}px, ${Math.floor(i / 2) * chipStep + (1 - k) * 10}px)`;
      e.style.opacity = k; e.classList.toggle('done', k >= 1);
    });
    return;
  }
  const pr = $('#pops').getBoundingClientRect(), vr = viewportEl.getBoundingClientRect();
  const ar = app.getBoundingClientRect(), fr = $('#pop-files').getBoundingClientRect();
  const sx = vr.width / VW, sy = vr.height / VH;
  const P = fileChips.map((e, i) => {
    const [px, py] = project(defs[EXPORT_ORDER[i]].parts[0].group.position);
    return { x: vr.left - pr.left + px * sx, y: vr.top - pr.top + py * sy };
  });
  if (!laneRank) { // lanes keep the parts' top-to-bottom order, so no two tracks cross
    laneRank = [];
    P.map((p, i) => i).sort((a, b) => P[a].y - P[b].y).forEach((i, r) => { laneRank[i] = r; });
  }
  const fx = fr.left - pr.left, fy = fr.top - pr.top, chipH = chipStep - 6;
  const xEnd = fx + fileChips[0].offsetWidth, xA = ar.left - pr.left - 4, xB = xEnd + Math.max(26, (xA - xEnd) * 0.22);
  const mid = (xA + xB) / 2;
  let out = '';
  fileChips.forEach((e, i) => {
    const r = laneRank[i], p = P[i], ly = fy + r * chipStep + chipH / 2;
    const k = seg(S.files, r / fileChips.length * 0.45, r / fileChips.length * 0.45 + 0.5);
    const arrive = seg(k, 0.82, 1);
    e.style.transform = `translate(${(1 - arrive) * 14}px, ${r * chipStep}px)`;
    e.style.opacity = arrive;
    e.classList.toggle('done', k >= 1);
    if (k <= 0) return;
    const d = `M${p.x.toFixed(1)} ${p.y.toFixed(1)}H${xA.toFixed(1)}C${mid.toFixed(1)} ${p.y.toFixed(1)} ${mid.toFixed(1)} ${ly.toFixed(1)} ${xB.toFixed(1)} ${ly.toFixed(1)}H${xEnd.toFixed(1)}`;
    const c = LANE_COLORS[i];
    out += `<path d="${d}" pathLength="1" stroke="${c}" stroke-dasharray="1" stroke-dashoffset="${(1 - k).toFixed(4)}"/>`
         + `<circle cx="${p.x.toFixed(1)}" cy="${p.y.toFixed(1)}" r="4" stroke="${c}"/>`;
  });
  splitSvg.innerHTML = out;
}

// Drawing: the contour is cut the way a shop would run it, holes first and the outer profile
// last, then the part lifts out of the sheet.
const contourEls = $$('#outline-svg .contours path'), kerfEls = $$('#outline-svg .kerf path');
const CUT_ORDER = [1, 2, 3, 4, 5, 0];
let cutLens = null;
function drawCut() {
  const d = seg(S.outline, 0, 1);
  contourEls.forEach(p => { p.style.strokeDasharray = '1'; p.style.strokeDashoffset = `${1 - d}`; });
  if (!cutLens) cutLens = kerfEls.map(p => p.getTotalLength());
  const total = CUT_ORDER.reduce((a, i) => a + cutLens[i], 0);
  let dist = S.cut * total, head = null;
  for (const i of CUT_ORDER) {
    const L = cutLens[i], k = clamp(dist / L);
    dist -= L;
    kerfEls[i].style.strokeDasharray = `${L}`;
    kerfEls[i].style.strokeDashoffset = `${L * (1 - k)}`;
    if (k > 0 && k < 1 && !head) head = kerfEls[i].getPointAtLength(k * L);
  }
  const hd = $('#cut-head');
  if (head) { hd.setAttribute('transform', `translate(${head.x} ${head.y})`); hd.style.opacity = 1; } else hd.style.opacity = 0;
  $('#outline-svg .kerf').style.opacity = 1 - S.lift * 0.75;
  $('#outline-svg .pocket').style.opacity = S.lift;
  const part = $('#cut-part');
  part.style.opacity = S.lift;
  part.setAttribute('transform', `translate(${-2.5 * S.lift} ${-3.5 * S.lift})`);
}

// Export: the Standalone preview shows one component on its own, framed to fit. Rendered once each.
const soloShots = {};
let solosTaken = false;
function takeSolos() {
  if (!renderer) return;
  solosTaken = true;
  const now = performance.now(), cam = camera.clone(), box = new THREE.Box3(), c = new THREE.Vector3(), sz = new THREE.Vector3();
  renderMode = 'solo';
  for (const n of EXPORT_ORDER) {
    soloDef = n;
    apply(now);
    box.makeEmpty(); defs[n].parts.forEach(p => box.expandByObject(p.mesh));
    box.getCenter(c); box.getSize(sz);
    cam.position.copy(c).addScaledVector(tmpV, 800); cam.up.copy(camera.up); cam.lookAt(c);
    cam.zoom = FRUSTUM / (Math.max(sz.x, sz.y, sz.z) * 1.6); cam.updateProjectionMatrix();
    renderer.render(scene, cam);
    soloShots[n] = renderer.domElement.toDataURL('image/png');
  }
  renderMode = null; soloDef = null;
  apply(now);
}

// View cube: its own small renderer that turns with the camera: labelled faces and an axis triad.
const viewCube = (() => {
  const el = $('#cube');
  if (!el) return null;
  let r;
  try { r = new THREE.WebGLRenderer({ canvas: el, antialias: true, alpha: true }); } catch (e) { return null; }
  r.setPixelRatio(1); r.setSize(200, 200, false);
  const sc = new THREE.Scene(), g = new THREE.Group(); sc.add(g);
  const tex = (draw) => { const cv = document.createElement('canvas'); cv.width = cv.height = 128; draw(cv.getContext('2d')); const t = new THREE.CanvasTexture(cv); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4; return t; };
  const face = label => new THREE.MeshBasicMaterial({ map: tex(x => {
    x.fillStyle = '#232838'; x.fillRect(0, 0, 128, 128);
    x.strokeStyle = '#6C7690'; x.lineWidth = 8; x.strokeRect(4, 4, 120, 120);
    x.fillStyle = '#F2F4F9'; x.font = '600 27px -apple-system, "Segoe UI", system-ui, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText(label, 64, 66);
  }) });
  g.add(new THREE.Mesh(new THREE.BoxGeometry(1, 1, 1), ['Right', 'Left', 'Top', 'Bottom', 'Front', 'Back'].map(face)));
  const o = new THREE.Vector3(-0.5, -0.5, -0.5);
  [[V(1, 0, 0), '#F97066', 'X'], [V(0, 1, 0), '#47CD89', 'Y'], [V(0, 0, 1), '#8B93F8', 'Z']].forEach(([d, col, L]) => {
    g.add(new THREE.ArrowHelper(d, o, 1.55, col, 0.2, 0.11));
    const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex(x => { x.fillStyle = col; x.font = '700 80px system-ui, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText(L, 64, 68); }), depthTest: false }));
    sp.position.copy(o).addScaledVector(d, 1.78); sp.scale.setScalar(0.32); g.add(sp);
  });
  const cam = new THREE.OrthographicCamera(-1.35, 1.35, 1.35, -1.35, 0.1, 20);
  return {
    draw(dir) {
      g.rotation.y = pivot.rotation.y;
      cam.position.copy(dir).multiplyScalar(6); cam.up.copy(camera.up); cam.lookAt(0, 0, 0);
      r.render(sc, cam);
    },
  };
})();

// Trim: hide parts in Inspect, then File > Export Assembly Without Hidden Parts. The app shows its
// notice; beside it, the file it writes is laid out: the same tree, with the hidden parts left out.
const OF_ROWS = [['Linear Stage', 0, 'grp'], ['Base Plate', 1], ['End Block  2x', 1], ['Guide Rod  2x', 1], ['Lead Screw', 1],
  ['M6 Screw  4x', 1, 'out'], ['Motor', 1, 'out'], ['Carriage', 1, 'grp'], ['Slider', 2], ['Bushing  4x', 2], ['Top Plate', 2], ['M5 Screw  4x', 2]];
$('#of-tree').innerHTML = OF_ROWS.map(([n, d, k]) => `<li class="${k || ''}" style="padding-left:${d * 12}px">${n}</li>`).join('');
const ofRows = $$('#of-tree li');
let keptShot = null, keptTaken = false;
function takeKept() {
  if (!renderer || keptTaken) return;
  keptTaken = true; // set first: the renders below run this code path again
  const now = performance.now();
  renderMode = 'kept'; apply(now);
  renderer.render(scene, camera);
  keptShot = renderer.domElement.toDataURL('image/png');
  renderMode = null; apply(now);
  const img = new Image(); img.alt = ''; img.src = keptShot;
  $('#of-thumb').replaceChildren(img);
}
function drawTrim() {
  vis($('#mac-menu'), S.menu);
  $('#mac-menu').style.transform = `translateY(${(1 - S.menu) * -6}px)`;
  const x = S.trimx, run = clamp(x / 0.8), done = x >= 0.8;
  vis($('#xnotice'), x > 0 ? 1 : 0);
  $('#xn-title').textContent = done ? 'Assembly exported' : 'Exporting the assembly';
  $('#xn-detail').textContent = done ? 'Saved to /Users/you/CAD/linear-stage without hidden parts.step, with 5 hidden parts left out.'
    : `${run < 0.55 ? 'Translating to STEP' : 'Writing the file'} · ${Math.round(run * 100)}%`;
  $('#xn-fill').style.width = `${run * 100}%`;
  $('#xn-bar').style.display = done ? 'none' : 'block';
  $('#xnotice').classList.toggle('done', done);
  vis($('#pop-onefile'), S.onefile);
  $('#pop-onefile').style.transform = `translateY(${(1 - S.onefile) * 16}px)`;
  if (S.onefile > 0.01 && !keptTaken) takeKept();
  ofRows.forEach((e, i) => e.classList.toggle('on', x > (i + 0.5) / ofRows.length * 0.8));
  $('#pop-onefile').classList.toggle('done', done);
}

// Present: two captures of the same view, taken in one go so the pose matches, then shown as an
// exploded stack over a deck slide that flattens into the finished slide.
let layersTaken = false;
function takeLayers() {
  if (!renderer) return;
  layersTaken = true;
  const now = performance.now(), shots = {};
  for (const mode of ['context', 'focus']) {
    renderMode = mode;
    apply(now);
    renderer.render(scene, camera);
    shots[mode] = renderer.domElement.toDataURL('image/png');
  }
  renderMode = null;
  apply(now);
  const [px, py] = project(V(0, 52, 0)); // top of the carriage
  const fx = px / VW * 100, fy = py / VH * 100;
  for (const mode of ['context', 'focus']) {
    const img = new Image();
    img.alt = '';
    img.src = shots[mode];
    $(`#ly-${mode}`).replaceChildren(img);
  }
  const lx = Math.min(fx + 18, 68), ly = Math.max(8, fy - 22); // label up and to the right, clear of the slide's text
  const line = $('#co-line');
  line.setAttribute('x1', fx); line.setAttribute('y1', fy); line.setAttribute('x2', lx); line.setAttribute('y2', ly);
  $('#co-dot').style.cssText = `left:${fx}%;top:${fy}%`;
  $('#co-label').style.cssText = `left:${lx}%;top:${ly}%`;
  $('#lstack').style.setProperty('--img-ar', (VW / VH).toFixed(4));
}

const lyEls = $$('#lstack .ly');
function drawLayers() {
  const t = S.layers, c = S.compose, x = 1 - c;
  vis($('#pop-layers'), clamp(t * 4));
  if (t <= 0.001) return;
  if (!layersTaken) takeLayers();
  const st = $('#lstack');
  st.style.setProperty('--x', x.toFixed(3));
  st.style.transform = `rotateX(${52 * x}deg) rotateZ(${-32 * x}deg) scale(${lerp(0.78, 1, c)})`;
  lyEls.forEach((e, i) => {
    const k = seg(t, i * 0.16, i * 0.16 + 0.5);
    e.style.opacity = k;
    e.style.transform = `translateZ(${i * 44 * x + (1 - k) * 140}px)`;
  });
}

// Explode: the exploded view's move handle on its own, on the top plate, to try. As in the app
// (ExplodeHandle.qml): an arrow along each world axis slides the part, a ring about each turns it,
// the ball moves it freely in the screen's plane; moves snap to 5 mm and turns to 15°, Shift
// releases the snap, and hit-testing happens in screen space (ExplodeHandleMath.qml). Until it is
// touched, the handle shows itself off: a slide along X, then a turn about Z.
const xhandle = (() => {
  const cv = $('#xh-gl'), read = $('#xh-read'), back = $('#xh-back');
  if (!cv) return null;
  let r;
  try { r = new THREE.WebGLRenderer({ canvas: cv, antialias: true, alpha: true }); } catch (e) { return null; }
  r.setClearColor(0x000000, 0); r.autoClear = false; r.setPixelRatio(Math.min(2, devicePixelRatio || 1));
  const sc = new THREE.Scene(), hs = new THREE.Scene();
  sc.add(new THREE.AmbientLight('#ffffff', 1.25));
  const sideLight = new THREE.DirectionalLight('#ffffff', 0.35); sideLight.position.set(-300, 300, -300); sc.add(sideLight);
  const cam = new THREE.OrthographicCamera(-1, 1, 1, -1, -1000, 1000);
  const keyL = new THREE.DirectionalLight('#ffffff', 1.9); keyL.position.set(0, 0, 1); cam.add(keyL); keyL.target.position.set(0, 0, -1); cam.add(keyL.target); sc.add(cam);
  const VIEW = new THREE.Vector3(1, 0.95, 1.3).normalize();
  cam.position.copy(VIEW).multiplyScalar(400); cam.lookAt(0, 0, 0);

  // The part: the top plate, centred on its middle, and a faint outline of where it started.
  const geom = new THREE.ExtrudeGeometry(plateShape(), { depth: PLATE.t, bevelEnabled: false, curveSegments: 24 }).rotateX(-Math.PI / 2).translate(0, -PLATE.t / 2, 0);
  const edgesG = new THREE.EdgesGeometry(geom, 28);
  const part = new THREE.Group();
  part.add(new THREE.Mesh(geom, new THREE.MeshStandardMaterial({ color: defs['Top Plate'].parts[0].warm, roughness: 0.6, metalness: 0 })));
  part.add(new THREE.LineSegments(edgesG, new THREE.LineBasicMaterial({ color: '#EEF2FF' })));
  sc.add(part);
  sc.add(new THREE.LineSegments(edgesG, new THREE.LineBasicMaterial({ color: '#8B93F8', transparent: true, opacity: 0.28 })));

  // The handle, drawn over the part (its own scene, after a depth clear), at the part's middle.
  const L = 40, RING = L * 0.72;
  const AX = [V(1, 0, 0), V(0, 1, 0), V(0, 0, 1)];
  const COL = ['#F97066', '#47CD89', '#8B93F8'], ACTIVE = '#FDE272', CENTRE = '#F2F4F9';
  const handle = new THREE.Group(); hs.add(handle);
  const mats = { move: [], rotate: [] };
  const basic = c => new THREE.MeshBasicMaterial({ color: c });
  AX.forEach((a, i) => {
    const q = new THREE.Quaternion().setFromUnitVectors(V(0, 1, 0), a);
    const m = basic(COL[i]); mats.move.push(m);
    const shaft = new THREE.Mesh(new THREE.CylinderGeometry(L * 0.04, L * 0.04, L * 0.62, 12), m);
    shaft.position.copy(a).multiplyScalar(L * 0.47); shaft.quaternion.copy(q);
    const tip = new THREE.Mesh(new THREE.ConeGeometry(L * 0.11, L * 0.24, 20), m);
    tip.position.copy(a).multiplyScalar(L * 0.86); tip.quaternion.copy(q);
    const rm = basic(COL[i]); rm.side = THREE.DoubleSide; mats.rotate.push(rm);
    const ring = new THREE.Mesh(new THREE.TorusGeometry(RING, L * 0.022, 8, 72), rm);
    ring.quaternion.setFromUnitVectors(V(0, 0, 1), a); // the torus lies about Z
    handle.add(shaft, tip, ring);
  });
  const ballM = basic(CENTRE);
  handle.add(new THREE.Mesh(new THREE.SphereGeometry(L * 0.095, 20, 14), ballM));

  let W = 1, H = 1, dirty = true, touched = false, drag = null, hover = null, demoT0 = 0, lastShown = false;
  const home = { p: new THREE.Vector3(), q: new THREE.Quaternion() };
  function size() {
    W = cv.clientWidth || 1; H = cv.clientHeight || 1;
    r.setSize(W, H, false);
    const worldH = Math.max(100, 175 / (W / H)), k = worldH / H; // the plate and its handle, with room to move
    cam.left = -W / 2 * k; cam.right = W / 2 * k; cam.top = worldH / 2 + 4; cam.bottom = -worldH / 2 + 4;
    cam.updateProjectionMatrix();
    dirty = true;
  }
  const proj = v => { const p = v.clone().project(cam); return { x: (p.x + 1) / 2 * W, y: (1 - p.y) / 2 * H }; };
  // Screen-space layout of the handle, as ExplodeHandleMath expects it.
  function layout() {
    const c = part.position;
    const u = new THREE.Vector3(), w = new THREE.Vector3();
    return {
      centre: proj(c),
      tips: AX.map(a => proj(a.clone().multiplyScalar(L * 0.98).add(c))),
      rings: AX.map(a => {
        u.set(a.y, a.z, a.x); w.crossVectors(a, u);
        return Array.from({ length: 48 }, (_, k) => {
          const t = k / 48 * Math.PI * 2;
          return proj(c.clone().addScaledVector(u, Math.cos(t) * RING).addScaledVector(w, Math.sin(t) * RING));
        });
      }),
    };
  }
  const distSeg = (px, py, ax, ay, bx, by) => {
    const dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    const t = l2 > 0 ? clamp(((px - ax) * dx + (py - ay) * dy) / l2) : 0;
    return Math.hypot(ax + t * dx - px, ay + t * dy - py);
  };
  function handleAt(x, y, lay) {
    if (Math.hypot(x - lay.centre.x, y - lay.centre.y) <= 12) return { kind: 'free', axis: -1 };
    let best = null, bd = Infinity;
    for (let a = 0; a < 3; a++) {
      const tip = lay.tips[a], dx = tip.x - lay.centre.x, dy = tip.y - lay.centre.y;
      if (Math.hypot(dx, dy) < 14) continue;
      const d = distSeg(x, y, lay.centre.x + dx * 0.15, lay.centre.y + dy * 0.15, tip.x, tip.y);
      if (d <= 10 && d < bd) { best = { kind: 'move', axis: a }; bd = d; }
    }
    if (best) return best;
    for (let a = 0; a < 3; a++) {
      const pts = lay.rings[a];
      let d = Infinity;
      pts.forEach((p, k) => { const q = pts[(k + 1) % pts.length]; d = Math.min(d, distSeg(x, y, p.x, p.y, q.x, q.y)); });
      if (d <= 8 && d < bd) { best = { kind: 'rotate', axis: a }; bd = d; }
    }
    return best;
  }
  function paint(active) {
    AX.forEach((_, i) => {
      mats.move[i].color.set(active?.kind === 'move' && active.axis === i ? ACTIVE : COL[i]);
      mats.rotate[i].color.set(active?.kind === 'rotate' && active.axis === i ? ACTIVE : COL[i]);
    });
    ballM.color.set(active?.kind === 'free' ? ACTIVE : CENTRE);
    dirty = true;
  }
  function readout(text, x, y) {
    read.classList.toggle('on', !!text);
    if (text) { read.textContent = text; read.style.transform = `translate(${Math.min(W - 90, x + 14).toFixed(0)}px, ${Math.max(4, y - 30).toFixed(0)}px)`; }
  }
  const fmtMM = v => `${v.toFixed(Math.abs(v) < 10 ? 2 : 1)} mm`;
  const snapTo = (v, step, free) => free ? v : Math.round(v / step) * step;
  const LIMIT = 40;
  const placed = () => part.position.lengthSq() > 1e-6 || Math.abs(part.quaternion.w) < 0.99999;

  const local = e => { const b = cv.getBoundingClientRect(); return { x: (e.clientX - b.left) * W / b.width, y: (e.clientY - b.top) * H / b.height }; };
  cv.addEventListener('pointerdown', e => {
    if (e.button !== 0) return;
    const { x, y } = local(e), lay = layout(), hit = handleAt(x, y, lay);
    if (!hit) return;
    e.preventDefault();
    touched = true; // the demo stops where it is; the reader carries on from there
    cv.setPointerCapture(e.pointerId); cv.classList.add('grabbing');
    drag = { ...hit, x0: x, y0: y, p0: part.position.clone(), q0: part.quaternion.clone(), lay,
      last: Math.atan2(y - lay.centre.y, x - lay.centre.x), turned: 0 };
    paint(hit);
  });
  cv.addEventListener('pointermove', e => {
    const { x, y } = local(e);
    if (!drag) {
      const h = handleAt(x, y, layout());
      if ((h?.kind) !== (hover?.kind) || (h?.axis) !== (hover?.axis)) { hover = h; if (touched) paint(h); }
      cv.classList.toggle('hot', !!h);
      return;
    }
    const free = e.shiftKey, d = drag, a = AX[d.axis];
    if (d.kind === 'move') {
      const tip = d.lay.tips[d.axis], sx = tip.x - d.lay.centre.x, sy = tip.y - d.lay.centre.y;
      let mm = ((x - d.x0) * sx + (y - d.y0) * sy) / (sx * sx + sy * sy) * L * 0.98;
      mm = snapTo(clamp(mm + d.p0.dot(a), -LIMIT, LIMIT), 5, free) - d.p0.dot(a);
      part.position.copy(d.p0).addScaledVector(a, mm);
      readout(fmtMM(mm), x, y);
    } else if (d.kind === 'rotate') {
      if (Math.hypot(x - d.lay.centre.x, y - d.lay.centre.y) > 8) {
        const ang = Math.atan2(y - d.lay.centre.y, x - d.lay.centre.x);
        let step = ang - d.last; step = Math.atan2(Math.sin(step), Math.cos(step));
        d.turned += step; d.last = ang;
      }
      const toward = Math.sign(a.dot(VIEW)) || 1;
      const deg = snapTo(-d.turned * 180 / Math.PI * toward, 15, free);
      part.quaternion.setFromAxisAngle(a, deg * Math.PI / 180).multiply(d.q0);
      readout(`${deg.toFixed(1)}°`, x, y);
    } else {
      const k = (cam.right - cam.left) / W;
      const right = V(1, 0, 0).applyQuaternion(cam.quaternion), up = V(0, 1, 0).applyQuaternion(cam.quaternion);
      const mv = right.multiplyScalar((x - d.x0) * k).addScaledVector(up, -(y - d.y0) * k);
      const p = d.p0.clone().add(mv);
      p.set(snapTo(clamp(p.x, -LIMIT, LIMIT), 5, free), snapTo(clamp(p.y, -LIMIT, LIMIT), 5, free), snapTo(clamp(p.z, -LIMIT, LIMIT), 5, free));
      part.position.copy(p);
      const f = v => v.toFixed(Math.abs(v) < 10 ? 2 : 1);
      readout(`X ${f(p.x)}  Y ${f(p.y)}  Z ${f(p.z)} mm`, x, y);
    }
    dirty = true;
  });
  const end = e => {
    if (!drag) return;
    drag = null; cv.classList.remove('grabbing');
    if (cv.hasPointerCapture?.(e.pointerId)) cv.releasePointerCapture(e.pointerId);
    readout('');
    paint(hover);
  };
  cv.addEventListener('pointerup', end); cv.addEventListener('pointercancel', end);
  cv.addEventListener('pointerleave', () => { if (!drag && hover) { hover = null; if (touched) paint(null); cv.classList.remove('hot'); } });
  // Put back: glide home, as the app's Put back does.
  let glide = null;
  const putBack = () => { touched = true; glide = { t0: performance.now(), p: part.position.clone(), q: part.quaternion.clone() }; readout(''); paint(null); };
  back.addEventListener('click', putBack);
  cv.addEventListener('dblclick', putBack);

  // The show-off loop, on its own clock while the handle is on screen and untouched.
  const ease = t => t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;
  function demo(t) {
    const c = t % 7.5;
    let mm = 0, deg = 0, act = null, text = '', at = null;
    if (c < 2.6) {
      if (c > 0.35) act = { kind: 'move', axis: 0 };
      const k = c < 0.6 ? 0 : c < 1.6 ? ease((c - 0.6) / 1.0) : c < 2.1 ? 1 : 1 - ease((c - 2.1) / 0.5);
      mm = c < 2.1 ? snapTo(30 * k, 5) : 30 * k;
      if (c >= 0.6 && c < 2.1) { text = fmtMM(mm); at = 'tip0'; }
      if (c >= 2.1) act = null;
    } else if (c > 3.0 && c < 5.6) {
      const d = c - 3.0;
      if (d > 0.35) act = { kind: 'rotate', axis: 2 };
      const k = d < 0.6 ? 0 : d < 1.6 ? ease((d - 0.6) / 1.0) : d < 2.1 ? 1 : 1 - ease((d - 2.1) / 0.5);
      deg = d < 2.1 ? snapTo(45 * k, 15) : 45 * k;
      if (d >= 0.6 && d < 2.1) { text = `${deg.toFixed(1)}°`; at = 'ring2'; }
      if (d >= 2.1) act = null;
    }
    part.position.set(mm, 0, 0);
    part.quaternion.setFromAxisAngle(AX[2], deg * Math.PI / 180);
    paint(act);
    if (text) {
      const lay = layout(), p = at === 'tip0' ? lay.tips[0] : lay.rings[2][6];
      readout(text, p.x, p.y);
    } else readout('');
  }

  return {
    size,
    frame(now, shown) {
      if (!shown) { if (lastShown) { lastShown = false; readout(''); } return; }
      if (!lastShown) { lastShown = true; demoT0 = now; if (W < 2) size(); }
      if (glide) {
        const k = ease(clamp((now - glide.t0) / 380));
        part.position.copy(glide.p).lerp(home.p, k);
        part.quaternion.copy(glide.q).slerp(home.q, k);
        if (k >= 1) glide = null;
        dirty = true;
      } else if (!touched) { demo((now - demoT0) / 1000); dirty = true; }
      back.classList.toggle('on', touched && placed());
      if (!dirty) return;
      dirty = false;
      handle.position.copy(part.position);
      r.clear(); r.render(sc, cam); r.clearDepth(); r.render(hs, cam);
    },
  };
})();

// Measure: the eight modes, as the Measure window lays them out, each with what it reads on this
// model. The one on the stage is lit (Radius, then Points); pointing at another shows what it does.
const MEASURE_MODES = (() => {
  const box = new THREE.Box3();
  pivot.updateMatrixWorld(true);
  parts.forEach(p => box.expandByObject(p.mesh));
  const sz = box.getSize(new THREE.Vector3());
  const n = v => Math.round(v).toLocaleString('en-US');
  return [
    ['point', 'Point to point', 'Points', '40.000 mm', 'Between two points. Snaps to vertices, circle centres and edges.',
      'M5.6 14.4 L14.4 5.6', 'M4 14.2 A1.8 1.8 0 1 0 4.01 14.2 Z M16 2.2 A1.8 1.8 0 1 0 16.01 2.2 Z', ''],
    ['surface-distance', 'Surface to surface', 'Surfaces', `${PLATE.t.toFixed(3)} mm`, 'Between two faces: the gap when they are parallel, else the closest distance.',
      'M3 4.5 H17 M3 15.5 H17 M10 7 V13 M8.2 8.8 L10 7 L11.8 8.8 M8.2 11.2 L10 13 L11.8 11.2', '', ''],
    ['angle', 'Angle', 'Angle', '90.0°', 'From one flat face or straight edge to another.',
      'M3.5 16 H16.5 M3.5 16 L12.5 4.5 M9.5 16 A6 6 0 0 0 7.2 11.3', '', ''],
    ['radius', 'Radius / diameter', 'Radius', 'R 9.000 mm', 'Of a circular edge, or a cylindrical or spherical face.',
      'M10 3 A7 7 0 1 0 10.01 3 Z M10 10 L15 5', 'M10 8.8 A1.2 1.2 0 1 0 10.01 8.8 Z', ''],
    ['edge', 'Edge / curve length', 'Edge', `${(PLATE.w - 2 * PLATE.r).toFixed(3)} mm`, 'Along an edge. Shift-click more edges to add them up.',
      'M4.5 15.5 L15.5 4.5 M2.8 13.8 L6.2 17.2 M13.8 2.8 L17.2 6.2', '', ''],
    ['area', 'Surface area', 'Area', `${n(plateArea)} mm²`, 'Of a face. Shift-click more faces to add them up.',
      'M4 4 H16 V16 H4 Z', '', 'M4 4 H16 V16 H4 Z'],
    ['volume', 'Volume', 'Volume', `${n(VOL_TOTAL)} mm³`, 'Of the selection, or of the whole model when nothing is selected.',
      'M10 2.8 L16.5 6.4 V13.6 L10 17.2 L3.5 13.6 V6.4 Z M3.5 6.4 L10 10 L16.5 6.4 M10 10 V17.2', '', 'M3.5 6.4 L10 10 V17.2 L3.5 13.6 Z'],
    ['bounds', 'Selected bounds', 'Bounds', `${n(sz.x)} × ${n(sz.y)} × ${n(sz.z)}`, 'The box around the selection, or around the whole model when nothing is selected.',
      'M3 7 V3 H7 M13 3 H17 V7 M17 13 V17 H13 M7 17 H3 V13 M7.5 7.5 H12.5 V12.5 H7.5 Z', '', ''],
  ].map(([key, label, short, value, hint, s, f, t]) => ({ key, label, short, value, hint, s, f, t }));
})();
$('#mg-well').innerHTML = MEASURE_MODES.map(m => `<button type="button" class="mg-t" role="listitem" tabindex="-1" data-k="${m.key}" title="${m.label}">`
  + `<svg viewBox="0 0 20 20" aria-hidden="true"><path d="${m.s}"/>${m.f ? `<path class="f" d="${m.f}"/>` : ''}${m.t ? `<path class="t" d="${m.t}"/>` : ''}</svg>`
  + `<b>${m.short}</b><small>${m.value}</small></button>`).join('');
const mgTiles = $$('#mg-well .mg-t');
let mgHover = null, mgShown = null;
mgTiles.forEach(t => {
  t.addEventListener('pointerenter', () => { mgHover = t.dataset.k; });
  t.addEventListener('focus', () => { mgHover = t.dataset.k; });
});
$('#mg-well').addEventListener('pointerleave', () => { mgHover = null; });
function drawMeasureModes() {
  vis($('#pop-measure'), seg(S.measure, 0, 0.15));
  if (S.measure <= 0) return;
  mgTiles.forEach((e, j) => {
    const k = seg(S.measure, 0.02 + j * 0.025, 0.14 + j * 0.025);
    e.style.opacity = k; e.style.transform = `translateY(${(1 - k) * 8}px)`;
  });
  const key = mgHover || (S.measure < 0.5 ? 'radius' : 'point');
  if (key === mgShown) return;
  mgShown = key;
  const m = MEASURE_MODES.find(x => x.key === key);
  mgTiles.forEach(e => e.classList.toggle('on', e.dataset.k === key));
  $('#mg-hint').innerHTML = `<b>${m.label}</b>${m.hint}`;
}

// ---------------------------------------------------------------------------------------------
// Layout: hero laptop -> screen fills the window -> app beside the copy
// ---------------------------------------------------------------------------------------------
let R0, R1, R2, rigBox, tabRects = [], railStep = 0, appScale = 0.5, effScale = 0.5;
const narrow = () => innerWidth < 900;
// How the story is laid out: 'side' (the copy beside the app) or 'stack' (the app above the copy).
// Shape decides as much as width: a landscape phone, or a foldable's screen turned sideways
// (iPhone Duo, either display), goes side by side, or the app would fill the screen and push the
// copy off it; a near-square screen (an unfolded Galaxy Z Fold, about 1:1.1) stacks, so the app
// can be drawn large enough to read instead of sitting small beside the copy.
const layoutMode = () => {
  const vw = innerWidth, vh = innerHeight, aspect = vw / vh;
  if (vw >= 1200 || aspect >= 1.25) return 'side';
  if (vw < 900) return 'stack';
  return aspect < 1.15 ? 'stack' : 'side';
};
const stacked = () => layoutMode() === 'stack';
// The app drawn at its compact size (980 wide, no side panels): wherever it is shown small.
const compactApp = () => narrow() || stacked();
// The smallest each scene's display is still worth showing at; with less room under the copy
// (a short screen) it is left out rather than cut off at the bottom of the screen.
const POP_MIN = { 'pop-race': 140, 'pop-layers': 110, 'pop-explode': 170, 'pop-measure': 130, 'pop-slice': 90, 'pop-mass': 150, 'pop-onefile': 170, 'pop-outline': 110 };

const POP_SCENE = { 'pop-race': 'speed', 'pop-layers': 'present', 'pop-explode': 'explode', 'pop-measure': 'measure', 'pop-slice': 'section', 'pop-mass': 'mass', 'pop-onefile': 'trim', 'pop-outline': 'drawing' };
const COPY_MAX = 520; // widest the scene copy and its cards get, in px (before the big-display scale)
function measure() {
  rig.style.transform = 'none';
  const vw = innerWidth, vh = innerHeight, nav = 52;
  const hero = $('#hero-copy');
  const rigTop = hero.offsetTop + hero.offsetHeight + Math.max(24, vh * 0.035);
  if (motion) {
    rig.style.setProperty('--rig-top', `${rigTop}px`);
    // the laptop (lid + base) is 0.5941 of its width tall: size it so all of it shows on load
    rig.style.setProperty('--lw', `${Math.max(300, Math.min(1120, vw * 0.84, (vh - rigTop - 18) / 0.5941))}px`);
  }
  const stack = stacked(), compact = compactApp();
  // the rail of workspace cards needs a wide app and the height under it
  const railOn = !compact && vh >= 600;
  document.documentElement.classList.toggle('stack', stack);
  document.documentElement.classList.toggle('no-rail', !railOn);
  app.classList.toggle('compact', compact);
  VW = viewportEl.clientWidth; VH = viewportEl.clientHeight; setFrustum();
  if (renderer) { renderer.setSize(VW, VH, false); renderer.render(scene, camera); }

  const sr = screenEl.getBoundingClientRect(), rr = rig.getBoundingClientRect(), tr = theater.getBoundingClientRect();
  rigBox = { x: rr.left - tr.left, y: rr.top - tr.top };
  R0 = { x: sr.left - tr.left, y: sr.top - tr.top, w: sr.width };
  appScale = sr.width / (compact ? 980 : 1728); // the app's own size: 1728 x 1117, a 16-inch MacBook Pro at its default scale
  app.style.setProperty('--as', appScale);
  if (renderer && !motion) { renderer.setPixelRatio(Math.min(3, Math.max(1, devicePixelRatio * appScale))); renderer.setSize(VW, VH, false); }
  effScale = 0.001; // force a buffer resize on the next placeRig()

  // R1: the screen fills the window
  const SA = 1.547; // screen aspect, 1728 / 1117
  const w1 = Math.min(vw * 0.94, (vh - nav - 28) * SA);
  R1 = { x: (vw - w1) / 2, y: nav + (vh - nav - w1 / SA) / 2, w: w1 };

  // R2: the stage position
  let cx, cy, cw, pz = 1;
  if (!stack) {
    const railH = railOn ? 64 + 18 : 0, small = vw < 900;
    // a small landscape screen splits about evenly, so the copy column stays readable
    const w2 = Math.min(vw * (small ? 0.5 : 0.56), (vh - nav - railH - (small ? 24 : 56)) * SA, 1500);
    // Copy column beside the stage, capped at a readable width; on a big display the pair is
    // centred as one block instead of pinned to the two edges with a gulf between them.
    const margin = small ? 16 : Math.max(24, vw * 0.035), gap = small ? 20 : clamp(vw * 0.04, 32, 96);
    // On a big display the stage grows with the window, so the copy and its cards grow too
    // (up to 1.4x), or they read like a phone layout beside it.
    pz = clamp(Math.min(vw / 1600, vh / 900), 1, 1.4);
    cw = Math.min(COPY_MAX * pz, vw - 2 * margin - gap - w2);
    cx = Math.max(margin, (vw - (cw + gap + w2)) / 2);
    const x2 = cx + cw + gap;
    const y2 = nav + (vh - nav - (w2 / SA + railH)) / 2;
    R2 = { x: x2, y: y2, w: w2 };
    // on a short screen the copy starts under the nav, not level with the app, to keep its room
    cy = vh < 700 ? nav + 14 : y2 + 8;
  } else {
    // The app across the top, at most about half the height so the copy has the rest. On a
    // phone that is the full width; on a near-square screen it is centred, with the copy
    // under it at the same width.
    const w2 = Math.min(vw - 24, (vh - nav) * 0.5 * SA);
    const x2 = (vw - w2) / 2;
    R2 = { x: x2, y: nav + 12, w: w2 };
    cx = x2 + 8; cw = w2 - 16; cy = R2.y + w2 / SA + 22;
  }
  const s = theater.style;
  s.setProperty('--pz', pz.toFixed(3));
  s.setProperty('--ax', `${R2.x}px`); s.setProperty('--ay', `${R2.y}px`);
  s.setProperty('--aw', `${R2.w}px`); s.setProperty('--ah', `${R2.w / SA}px`);
  s.setProperty('--cx', `${cx}px`); s.setProperty('--cy', `${cy}px`); s.setProperty('--cw', `${cw}px`);
  const ch = Math.max(...$$('.sc').map(e => e.offsetHeight)) + (narrow() ? 14 : 32);
  s.setProperty('--ch', `${ch}px`);

  // Each scene's display sits just under that scene's own text and is sized to what is left.
  const limit = innerHeight - (narrow() ? 12 : 22);
  const below = scene => cy + ($(`.sc[data-scene="${scene}"]`)?.offsetHeight || 0) + (narrow() ? 12 : 26);
  for (const [id, scene] of Object.entries(POP_SCENE)) {
    const el = $('#' + id), top = below(scene);
    el.style.top = `${top}px`;
    el.style.setProperty('--avail', `${Math.max(110, limit - top)}px`);
    if (id === 'pop-mass') el.classList.toggle('tight', limit - top < 370);
    el.classList.toggle('no-room', limit - top < POP_MIN[id]);
  }
  const ft = below('split'), benchH = $('#pop-bench').offsetHeight || 140;
  const rowsN = stack ? 4 : 8;
  chipStep = stack ? 30 : Math.max(22, Math.min(34, Math.floor((limit - ft - benchH - 12) / 8)));
  $('#pop-files').style.top = `${ft}px`;
  $('#pop-files').style.setProperty('--chip-h', `${chipStep - 6}px`);
  $('#pop-files').style.height = `${chipStep * rowsN}px`;
  $('#pop-bench').style.top = `${ft + chipStep * rowsN + 12}px`;
  // the exported files, and the race under them, only where they fit
  $('#pop-files').classList.toggle('no-room', limit - ft < chipStep * rowsN);
  splitSvg.classList.toggle('no-room', limit - ft < chipStep * rowsN);
  $('#pop-bench').classList.toggle('no-room', limit - ft < chipStep * rowsN + 12 + benchH);
  laneRank = null;
  xhandle?.size();

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
  document.querySelector('.nav').classList.toggle('in-story', S.k > 1.6 && !!storyST?.isActive);
  $('#vp-hint').style.setProperty('--k', (1 - seg(S.k, 0, 0.25)).toFixed(3));
  // keep the WebGL buffer sharp at whatever size the screen is shown
  const eff = appScale * s;
  if (renderer && Math.abs(eff - effScale) / effScale > 0.08) {
    effScale = eff;
    renderer.setPixelRatio(Math.min(3, Math.max(1, devicePixelRatio * eff)));
    renderer.setSize(VW, VH, false);
    renderer.render(scene, camera); // a resize clears the canvas; redraw now, not next frame
  }
}

// ---------------------------------------------------------------------------------------------
// The scroll story
// ---------------------------------------------------------------------------------------------
// Trim comes before split: it happens in Inspect, where the scenes before it already are, so the
// story changes workspace once for Export and once for Drawing instead of hopping back and forth.
const SCENES = ['workspaces', 'speed', 'present', 'explode', 'section', 'measure', 'mass', 'trim', 'split', 'drawing'];
// split runs 2.3 so it can rest after its export race finishes (split + 1.8) and still before its
// copy starts to fade (drawing - 0.3).
const T = { workspaces: 1.75, speed: 4.15, present: 5.5, explode: 7.4, section: 8.7, measure: 10, mass: 11.3, trim: 12.9, split: 15.0, drawing: 17.3, end: 19.8 };

let storyST = null, storyTL = null;
const navWs = $$('.nav a[data-ws]');

// Scene stepping. The story is a stack of one-screen snap points (native CSS scroll snap, like a
// paged site): the browser does the snapping, so wheel, trackpad, touch and keys all feel native,
// and scroll-snap-stop keeps a flick from skipping scenes. Scroll only picks WHICH scene is on;
// the scene then plays on its own clock, eased, instead of being scrubbed by the scroll position.
let sceneIdx = 0, sceneTween = null, jumping = false;
// Deep link to a scene: ?scene=5 opens on the sixth snap point (also used for layout checks).
let deepScene = +new URLSearchParams(location.search).get('scene') || 0;
const PAGE_H = () => innerHeight; // one scene = one screen of scroll
const sceneAtScroll = () => clamp(Math.round((scrollY - storyST.start) / PAGE_H()), 0, REST_TIMES().length - 1);
function playScene(i) {
  const R = REST_TIMES();
  i = clamp(i, 0, R.length - 1);
  if (i === sceneIdx) return;
  sceneIdx = i;
  const hurried = !!sceneTween?.isActive(); // a new step before the last one finished playing
  sceneTween?.kill();
  const from = storyTL.time(), to = R[i];
  const dir = Math.sign(to - from) || 1;
  // Where this step starts: the rest point one scene back along the way the reader is going.
  const start = R[clamp(i - dir, 0, R.length - 1)];
  // Scenes the playhead would have to pass through before that start, if a fast scroll outran it.
  const behind = R.filter(r => (dir > 0 ? r > from + 0.01 && r < start - 0.01 : r < from - 0.01 && r > start + 0.01)).length
    + (Math.abs(from - start) > 0.01 && Math.sign(start - from) === dir ? 1 : 0);
  const secs = span => clamp(span * SCENE_PACE, 0.8, 3.2);
  const seq = gsap.timeline();
  let t = from;
  if (behind >= 2) {
    // Several scenes behind: don't replay them. Fade, land on the scene before the target, and play
    // only the last step, so the reader arrives about as fast as they scrolled.
    sceneTween = seq;
    seq.pause();
    fadeTo(() => { if (sceneTween !== seq) return; storyTL.time(start); seq.restart(); });
    t = start;
  } else if (behind === 1) {
    // One scene behind: finish it quickly, then play the step the reader asked for.
    seq.to(storyTL, { time: start, duration: clamp(Math.abs(start - from) * 0.25, 0.15, 0.4), ease: 'power1.in' });
    t = start;
  }
  // Playing forward at a reading pace, stop on any hold point on the way so a step can be read
  // before the next one. Not when the reader is hurrying past.
  const holds = to > t && !hurried && !behind ? SCENE_HOLDS().filter(h => h.at > t + 0.01 && h.at < to - 0.01) : [];
  holds.forEach(h => {
    seq.to(storyTL, { time: h.at, duration: secs(h.at - t), ease: 'sine.inOut' });
    seq.to({}, { duration: h.secs });
    t = h.at;
  });
  // A step that follows a hurried one plays a little quicker, so a run of quick scrolls stays quick.
  seq.to(storyTL, { time: to, duration: secs(Math.abs(to - t)) * (hurried ? 0.6 : 1), ease: hurried ? 'power2.out' : 'sine.inOut' });
  sceneTween = seq;
}
// Seconds of playback per unit of story time: unhurried, so each step can be followed.
const SCENE_PACE = 1.25;
// Moments worth a pause on the way into a scene (story time, and how long to hold, in seconds).
const SCENE_HOLDS = () => [
  { at: T.present + 0.88, secs: 1.4 }, // the captures fanned out as layers, before they stack onto the slide
];
// Fade the theatre out, run `mid` (which sets the story somewhere new), and fade back in. A fade
// already under way is shared: `mid` then runs at once, behind it.
function fadeTo(mid) {
  if (jumping) { mid(); return; }
  jumping = true;
  theater.animate([{ opacity: 1 }, { opacity: 0 }], { duration: 120, easing: 'ease-in', fill: 'forwards' }).finished.then(() => {
    mid();
    return theater.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 220, easing: 'ease-out', fill: 'forwards' }).finished;
  }).finally(() => { jumping = false; });
}
// Land on a scene without playing the ones in between: fade out, set it, fade back in.
function jumpToScene(i) {
  const R = REST_TIMES();
  i = clamp(i, 0, R.length - 1);
  sceneIdx = i;
  sceneTween?.kill();
  sceneTween = null;
  fadeTo(() => { if (!sceneTween) storyTL.time(R[sceneIdx]); }); // the latest target, if the scroll moved on during the fade
}
function syncScene() {
  if (!storyST) return;
  const i = sceneAtScroll();
  if (i === sceneIdx) return;
  // One step is a scene change: play it. More than one (the scrollbar, Home/End, a link) jumps.
  if (Math.abs(i - sceneIdx) > 1) jumpToScene(i); else playScene(i);
}
// One snap point per scene, laid over the pinned stretch of the page.
// After the benchmarks the page reads as an ordinary document, with sections taller than a screen:
// one snap area covers everything from "Any CAD" to the end. A snap area taller than the screen
// lets the page rest anywhere inside it, so that stretch scrolls freely, and nothing in it can be
// skipped by a snap that overshoots.
function layoutSnaps() {
  const box = $('#snaps'), n = REST_TIMES().length;
  if (box.children.length !== n + 1) box.innerHTML = '<i></i>'.repeat(n) + '<i class="snap-free"></i>';
  const els = [...box.children], tail = els.pop();
  els.forEach((e, k) => { e.style.top = `${storyST.start + k * PAGE_H()}px`; e.style.height = `${PAGE_H()}px`; });
  tail.style.height = '0px';
  const top = $('#story').getBoundingClientRect().top + scrollY;
  tail.style.top = `${top}px`;
  tail.style.height = `${Math.max(PAGE_H(), document.documentElement.scrollHeight - top)}px`;
}
// A link into the story (rail cards, workspace tabs, nav): scroll to the first scene at or after t.
function scrollToTime(t) {
  if (!storyST) return;
  const R = REST_TIMES();
  let i = R.findIndex(r => r >= t);
  if (i < 0) i = R.length - 1;
  jumpToScene(i);
  window.scrollTo({ top: storyST.start + i * PAGE_H(), behavior: 'instant' });
}
const WS_JUMP = { inspect: () => T.speed, export: () => T.split, drawing: () => T.drawing };
function bindWorkspaceJumps() {
  [...railCards, ...tabEls, ...navWs].forEach(e => {
    const ws = e.dataset.ws || e.dataset.tab;
    e.addEventListener('click', ev => { ev.preventDefault(); scrollToTime(WS_JUMP[ws]()); });
  });
}
// Where each scene is fully shown and holding (timeline seconds). Hero, the three steps of the
// workspace tour, then every scene once its animation has finished.
const REST_TIMES = () => [
  0,
  T.workspaces + 0.25, T.workspaces + 1.0, T.workspaces + 1.55,
  T.speed + 0.95, T.present + 1.45, T.explode + 0.87, T.section + 1.0, T.measure + 0.9,
  T.mass + 0.9, T.trim + 1.6, T.split + 1.85, T.drawing + 2.6, T.end + 0.6, // split: once the export race (ends +1.8) has finished
];
function buildTimeline() {
  theater.classList.add('motion');
  document.documentElement.classList.add('motion');
  document.documentElement.classList.remove('preload'); // see the inline script in index.html
  const tl = storyTL = gsap.timeline({ defaults: { ease: 'none' }, paused: true });
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

  // workspaces: a quick tour of the three tabs, then back to Inspect for the details
  to({ ws: 1 }, T.workspaces + 0.55, 0.3, 'power3.inOut');
  to({ ws: 2 }, T.workspaces + 1.15, 0.3, 'power3.inOut');
  to({ ws: 0 }, T.speed - 0.45, 0.35, 'power3.inOut');

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
  to({ layers: 1 }, T.present + 0.4, 0.45, 'power2.out');
  to({ compose: 1 }, T.present + 1.0, 0.32, 'power2.inOut');
  to({ layers: 0, compose: 0, capture: 0, ghost: 0 }, T.explode - 0.3, 0.2);

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
  to({ mass: 0 }, T.trim - 0.55, 0.2);

  // trim, still in Inspect
  to({ trim: 1 }, T.trim + 0.05, 0.35, 'power2.inOut');     // hide Motor and the M6 screws
  to({ menu: 1 }, T.trim + 0.45, 0.12, 'power2.out');       // File > Export Assembly Without 5 Hidden Parts…
  to({ menu: 0 }, T.trim + 0.72, 0.08);
  to({ onefile: 1 }, T.trim + 0.75, 0.2, 'power3.out');
  to({ trimx: 1 }, T.trim + 0.8, 0.85, 'none');
  to({ onefile: 0, trimx: 0 }, T.split - 0.45, 0.15);

  // -> Export workspace (horizontal); the hidden parts come back on the way
  to({ ws: 1, trim: 0 }, T.split - 0.35, 0.35, 'power3.inOut');
  to({ split: 1 }, T.split + 0.05, 0.6, 'none');
  to({ explode: 0.6 }, T.split + 0.5, 0.35, 'power2.inOut');
  to({ files: 1 }, T.split + 0.6, 0.65, 'none');
  to({ bench: 1 }, T.split + 1.2, 0.6, 'none');
  to({ files: 0, benchOut: 1, explode: 0 }, T.drawing - 0.25, 0.2); // fades the race out finished, rather than running it backwards

  // -> Drawing workspace (horizontal)
  to({ ws: 2 }, T.drawing - 0.35, 0.35, 'power3.inOut');
  to({ iso: 1, rot: 0 }, T.drawing, 0.4);
  to({ outline: 1 }, T.drawing + 0.55, 0.5, 'power1.inOut');
  to({ cut: 1 }, T.drawing + 1.1, 0.95, 'none');
  to({ lift: 1 }, T.drawing + 2.1, 0.3, 'power2.out');
  tl.to({}, { duration: 0.6 }, T.end);

  storyST = ScrollTrigger.create({
    trigger: theater,
    start: 'top top',
    // one screen of scroll per scene: each is a snap point (see layoutSnaps), and the timeline is
    // played between them rather than scrubbed (see playScene)
    end: () => `+=${(REST_TIMES().length - 1) * PAGE_H()}`,
    pin: true,
    pinSpacer: '#pin-wrap',
    invalidateOnRefresh: true,
    onRefresh: self => {
      storyST = self;
      layoutSnaps();
      document.documentElement.classList.add('snap'); // only now: see html.snap in v2.css
      measure(); apply();
      if (deepScene > 0) { // ?scene=N, once there are snap points to land on
        const d = clamp(deepScene, 0, REST_TIMES().length - 1);
        deepScene = 0;
        setTimeout(() => { sceneIdx = d; tl.time(REST_TIMES()[d]); window.scrollTo({ top: self.start + d * PAGE_H(), behavior: 'instant' }); }, 0);
        return;
      }
      // a reload part-way down, or a resize: land on the scene at this scroll position, unplayed
      const i = sceneAtScroll();
      if (i !== sceneIdx) { sceneIdx = i; sceneTween?.kill(); tl.time(REST_TIMES()[i]); }
    },
  });
  tl.eventCallback('onUpdate', () => { placeRig(); drawHero(); homeOrbit(); });
  addEventListener('scroll', syncScene, { passive: true });
}

// Fallback: no scrubbing. Each scene's state is set when its text comes into view.
const STILL = {
  workspaces: {},
  speed: {}, present: { warm: 1, ghost: 1, capture: 1, layers: 1, compose: 1 }, explode: { exWin: 1, explode: 1, rot: 0.95 },
  section: { sec: 1, secPos: 0, slice: 1, rot: -0.45 }, measure: { measure: 1 }, mass: { mass: 1 },
  split: { ws: 1, split: 1 }, trim: { ws: 0, trim: 1, onefile: 1, trimx: 1 }, drawing: { ws: 2, iso: 1, outline: 1, cut: 1, lift: 1, rot: 0 },
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
  // On GSAP's clock (delayedCall), not setInterval: a timer keeps firing while the page's animation
  // frames are paused (a background tab or a hidden pane), and the queued swaps then pile up into
  // several half-faded lines on top of each other. Each swap also settles every other line first.
  let i = 0;
  const swap = () => {
    if (S.heroOut <= 0.5) {
      const a = lines[i], b = lines[(i + 1) % lines.length];
      gsap.killTweensOf(lines);
      lines.forEach(l => { if (l !== a && l !== b) gsap.set(l, { autoAlpha: 0, yPercent: 100 }); });
      gsap.set(a, { autoAlpha: 1, yPercent: 0 });
      gsap.to(a, { autoAlpha: 0, yPercent: -100, duration: 0.45, ease: 'power2.in' });
      gsap.fromTo(b, { autoAlpha: 0, yPercent: 100 }, { autoAlpha: 1, yPercent: 0, duration: 0.6, ease: 'power3.out', delay: 0.2 });
      i = (i + 1) % lines.length;
    }
    gsap.delayedCall(2.6, swap);
  };
  gsap.delayedCall(2.6, swap);
}

// ---------------------------------------------------------------------------------------------
// Render loop: only while the theatre is on screen
// ---------------------------------------------------------------------------------------------
// Drag the model in the viewport to turn it. Vertical page scroll still works on touch screens.
(function bindOrbit() {
  const skip = e => e.button !== 0 || e.target.closest('.tw, .toolbar, .vp-drops');
  const hint = $('#vp-hint');
  viewportEl.addEventListener('pointerdown', e => {
    if (skip(e)) return;
    Object.assign(orbit, { dragging: true, homing: false, vYaw: 0, vPitch: 0, x: e.clientX, y: e.clientY, lt: performance.now() });
    if (!orbit.touched) { orbit.touched = true; hint?.classList.add('gone'); }
    viewportEl.setPointerCapture(e.pointerId);
    viewportEl.classList.add('grabbing');
  });
  viewportEl.addEventListener('pointermove', e => {
    if (!orbit.dragging) return;
    const r = viewportEl.getBoundingClientRect(), now = performance.now();
    const dx = (e.clientX - orbit.x) / r.width, dy = (e.clientY - orbit.y) / r.height;
    const dts = Math.max(8, now - orbit.lt) / 1000;
    orbit.x = e.clientX; orbit.y = e.clientY; orbit.lt = now;
    const dYaw = dx * Math.PI * 1.5, dPitch = dy * 2.2;
    orbit.yaw += dYaw;
    orbit.pitch = Math.min(PITCH_MAX, Math.max(PITCH_MIN, orbit.pitch + dPitch));
    orbit.vYaw = dYaw / dts; orbit.vPitch = dPitch / dts;
  });
  const end = e => {
    if (!orbit.dragging) return;
    orbit.dragging = false;
    if (performance.now() - orbit.lt > 80) orbit.vYaw = orbit.vPitch = 0; // held still before letting go: no flick
    viewportEl.classList.remove('grabbing');
    if (viewportEl.hasPointerCapture?.(e.pointerId)) viewportEl.releasePointerCapture(e.pointerId);
  };
  viewportEl.addEventListener('pointerup', end);
  viewportEl.addEventListener('pointercancel', end);
  viewportEl.addEventListener('dblclick', e => { if (!skip(e)) homeOrbit(); });
})();

// Play a section's animation only once the whole section is on screen (or it fills the screen),
// after a short pause, so it runs when the reader is looking at it.
function whenFullyInView(section, run) {
  let timer = null;
  const io = new IntersectionObserver(([e]) => {
    const full = e.intersectionRatio > 0.9 || (e.rootBounds && e.intersectionRect.height >= e.rootBounds.height * 0.85);
    clearTimeout(timer);
    if (full) timer = setTimeout(() => { run(); io.disconnect(); }, 350);
  }, { threshold: [0, 0.25, 0.5, 0.7, 0.8, 0.9, 0.95, 1] });
  io.observe(section);
}
whenFullyInView($('#flow'), () => $('#flow').classList.add('in'));
whenFullyInView($('#private'), () => $('#lock').classList.add('in'));

let renderActive = true;
new IntersectionObserver(([e]) => { renderActive = e.isIntersecting; }).observe(theater);
function loop(now) {
  if (renderActive && !document.hidden) apply(now);
  requestAnimationFrame(loop);
}

// ---------------------------------------------------------------------------------------------
// Downloads: point each button at its zip in the latest release.
// ---------------------------------------------------------------------------------------------
// The visitor's platform, from the same check the download buttons use: it picks the highlighted
// download and the copy shortcut's modifier key (Cmd on Apple devices, Ctrl everywhere else).
const PLATFORM_UA = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || navigator.userAgent;
const IS_WINDOWS = /win/i.test(PLATFORM_UA);
const IS_APPLE = !IS_WINDOWS && /mac|iphone|ipad|ipod/i.test(PLATFORM_UA);
$$('kbd[data-mod]').forEach(k => { k.textContent = IS_APPLE ? '\u2318' : 'Ctrl'; k.title = IS_APPLE ? 'Command' : 'Control'; });

function downloads() {
  const buttons = $$('[data-downloads] .dl');
  const mine = IS_WINDOWS ? 'windows-x64' : 'macos-arm64';
  buttons.forEach(b => { if (b.dataset.asset === mine) b.classList.add('primary'); });
  fetch('https://api.github.com/repos/imperator28/Loupe/releases/latest', { headers: { Accept: 'application/vnd.github+json' } })
    .then(r => { if (!r.ok) throw new Error(r.status); return r.json(); })
    .then(rel => {
      buttons.forEach(b => {
        const suffix = `-${b.dataset.asset}.zip`;
        const asset = (rel.assets || []).find(a => a.name.endsWith(suffix));
        if (asset) { b.href = asset.browser_download_url; ASSET_NAMES[b.dataset.asset] = asset.name; }
      });
      if (rel.tag_name) $$('[data-version]').forEach(e => { e.textContent = `Loupe ${rel.tag_name}`; });
      if (rel.html_url) $$('[data-notes]').forEach(e => { e.href = rel.html_url; });
    })
    .catch(() => {});
}

// ---------------------------------------------------------------------------------------------
// Wayfinding: a section indicator on the right edge and a back-to-top button. Both stay out of
// sight until scrolling asks for them: the indicator shows while the page moves (and on hover),
// the button after a light scroll up past the first screen.
// ---------------------------------------------------------------------------------------------
function wayfinding() {
  const SECTIONS = ['Home', 'Workspaces', 'Open fast', 'Present', 'Explode', 'Section', 'Measure', 'Mass', 'Trim', 'Export', 'Drawing',
    'Benchmarks', 'Any CAD', 'Private', 'Download'];
  const STORY_N = 11; // the first 11 live in the pinned story; the rest are ordinary sections
  const STOP_SECTION = [0, 1, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10]; // story snap point -> section
  const LOWER = ['#measured', '#story', '#private', '#download'].map(s => $(s));
  const pad = n => String(n).padStart(2, '0');

  const nav = document.createElement('nav');
  nav.className = 'secnav';
  nav.setAttribute('aria-label', 'Sections');
  nav.innerHTML = `<p class="secnav-label" aria-hidden="true"><span><b></b> / ${pad(SECTIONS.length)}</span><em></em></p>`
    + `<ol>${SECTIONS.map((n, i) => `<li><button type="button" aria-label="${n}" data-i="${i}"><i></i></button></li>`).join('')}</ol>`;
  const top = document.createElement('button');
  top.type = 'button';
  top.className = 'totop';
  top.setAttribute('aria-label', 'Back to top');
  top.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 19V5m-6 6 6-6 6 6"/></svg>';
  document.body.append(nav, top);
  const ticks = $$('button', nav), label = $('.secnav-label b', nav), name = $('.secnav-label em', nav);

  const current = () => {
    if (storyST && scrollY < storyST.end + innerHeight * 0.5) return STOP_SECTION[sceneAtScroll()] ?? STORY_N - 1;
    let s = storyST ? STORY_N - 1 : 0; // without the story (reduced motion), count from Home
    LOWER.forEach((el, k) => { if (el && el.getBoundingClientRect().top <= innerHeight * 0.4) s = STORY_N + k; });
    return s;
  };
  let shown = -1, peeking = -1;
  const box = $('.secnav-label', nav);
  const say = s => {
    label.textContent = pad(s + 1); name.textContent = SECTIONS[s];
    const tk = ticks[s]; // keep the label level with the tick it describes
    box.style.setProperty('--ly', `${tk.offsetTop + tk.offsetHeight / 2 - box.offsetHeight / 2}px`);
  };
  const mark = () => {
    const s = current();
    if (s !== shown) { shown = s; ticks.forEach((t, i) => t.toggleAttribute('aria-current', i === s)); }
    if (peeking < 0) say(s); // every time: fonts and layout can settle after the first placement
  };
  // Pointing at a tick previews where it goes; moving off shows the current section again.
  const peek = i => { peeking = i; say(i); nav.classList.toggle('peek', i !== shown); };
  const unpeek = () => { peeking = -1; say(shown); nav.classList.remove('peek'); };
  // The preview follows the pointer from tick to tick and only lets go when the pointer leaves the
  // whole column, so passing over the space between two ticks never flicks back to the current one.
  ticks.forEach((t, i) => {
    t.addEventListener('pointerenter', () => peek(i));
    t.addEventListener('focus', () => peek(i));
    t.addEventListener('blur', unpeek);
  });
  $('ol', nav).addEventListener('pointerleave', unpeek);

  const goSection = i => {
    if (i < STORY_N) {
      if (!storyST) return;
      const stop = STOP_SECTION.indexOf(i);
      jumpToScene(stop);
      window.scrollTo({ top: storyST.start + stop * PAGE_H(), behavior: 'instant' });
    } else {
      LOWER[i - STORY_N]?.scrollIntoView({ behavior: reduceMotion ? 'auto' : 'smooth' });
    }
  };
  ticks.forEach(t => t.addEventListener('click', () => goSection(+t.dataset.i)));
  top.addEventListener('click', () => { goSection(0); hideTop(); top.blur(); });

  let lastY = scrollY, upBy = 0, navT = 0, topT = 0;
  const hideTop = () => { top.classList.remove('show'); upBy = 0; };
  addEventListener('scroll', () => {
    const y = scrollY, dy = y - lastY;
    lastY = y;
    mark();
    const away = y > innerHeight * 0.6;
    nav.classList.toggle('show', away);
    clearTimeout(navT);
    navT = setTimeout(() => nav.classList.remove('show'), 1600);
    if (dy < 0) upBy -= dy; else if (dy > 0) upBy = 0;
    if (!away || dy > 24) { hideTop(); return; }
    if (upBy > 24) {
      top.classList.add('show');
      clearTimeout(topT);
      topT = setTimeout(hideTop, 3200);
    }
  }, { passive: true });
  mark();
  document.fonts?.ready.then(() => { if (peeking < 0) say(shown); }); // the label's size changes once the fonts load
}

// ---------------------------------------------------------------------------------------------
// Ambient glow behind the page: one light eases after a mouse cursor, one drifts on its own (so
// touch screens and a still cursor get movement too). Transform-only, one rAF, paused with the tab.
// ---------------------------------------------------------------------------------------------
function ambientGlow() {
  const glow = document.createElement('div');
  glow.className = 'glow';
  glow.setAttribute('aria-hidden', 'true');
  glow.innerHTML = '<i class="g-drift"></i><i class="g-cursor"></i>';
  document.body.prepend(glow);
  const cur = $('.g-cursor', glow), drift = $('.g-drift', glow);
  const at = (el, x, y) => { el.style.transform = `translate3d(${x.toFixed(1)}px, ${y.toFixed(1)}px, 0)`; };
  if (reduceMotion) { at(drift, innerWidth * 0.5, innerHeight * 0.3); return; } // a still glow, no following
  let tx = innerWidth / 2, ty = innerHeight / 3, x = tx, y = ty, seen = false;
  addEventListener('pointermove', e => {
    if (e.pointerType !== 'mouse') return;
    tx = e.clientX; ty = e.clientY;
    if (!seen) { seen = true; x = tx; y = ty; glow.classList.add('has-cursor'); }
  }, { passive: true });
  document.addEventListener('mouseout', e => { if (!e.relatedTarget) { seen = false; glow.classList.remove('has-cursor'); } });
  const t0 = performance.now();
  const frame = now => {
    const t = (now - t0) / 1000;
    x += (tx - x) * 0.07; y += (ty - y) * 0.07; // eases after the cursor rather than sticking to it
    at(cur, x, y);
    at(drift, innerWidth * (0.5 + 0.34 * Math.sin(t * 0.09)), innerHeight * (0.42 + 0.3 * Math.sin(t * 0.067 + 1.3)));
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
}

// ---------------------------------------------------------------------------------------------
// After a download: the first open, step by step, for the platform that was downloaded. The
// download itself goes ahead as the link says; the guide opens over the page beside it.
// ---------------------------------------------------------------------------------------------
const ASSET_NAMES = {}; // data-asset -> the zip's file name in the latest release, once known
function firstRunGuide() {
  const dg = $('#dg');
  if (!dg || !dg.showModal) return;
  const plats = $$('.dg-plat [data-plat]', dg), dots = $('.dg-dots', dg);
  const back = $('[data-step="-1"]', dg), next = $('[data-step="1"]', dg);
  let plat = 'mac', step = 0;
  const steps = () => $$(`.dg-steps[data-plat="${plat}"] li`, dg);
  const show = () => {
    const list = steps();
    step = clamp(step, 0, list.length - 1);
    dg.dataset.plat = plat;
    plats.forEach(b => { const on = b.dataset.plat === plat; b.setAttribute('aria-selected', on); b.tabIndex = on ? 0 : -1; });
    list.forEach((li, i) => { li.classList.toggle('on', i === step); li.classList.toggle('done', i < step); });
    $$('.dg-art', dg).forEach(a => a.classList.toggle('on', a.dataset.art === `${plat}-${step}`));
    dots.innerHTML = list.map((_, i) => `<i class="${i === step ? 'on' : ''}"></i>`).join('');
    back.disabled = step === 0;
    next.textContent = step === list.length - 1 ? 'Done' : 'Next';
  };
  const go = d => { if (d > 0 && step === steps().length - 1) { dg.close(); return; } step += d; show(); };
  back.addEventListener('click', () => go(-1));
  next.addEventListener('click', () => go(1));
  plats.forEach(b => b.addEventListener('click', () => { plat = b.dataset.plat; step = 0; show(); }));
  $$('.dg-steps', dg).forEach(ol => $$('li', ol).forEach((li, i) => { li.dataset.n = i + 1; li.addEventListener('click', () => { step = i; show(); }); }));
  $('[data-close]', dg).addEventListener('click', () => dg.close());
  dg.addEventListener('click', e => { if (e.target === dg) dg.close(); }); // a click on the backdrop
  dg.addEventListener('keydown', e => {
    if (e.target.closest('button') && (e.key === 'Enter' || e.key === ' ')) return;
    if (e.key === 'ArrowRight') { e.preventDefault(); if (step < steps().length - 1) go(1); }
    if (e.key === 'ArrowLeft') { e.preventDefault(); if (step > 0) go(-1); }
  });
  // Scroll snapping on the page underneath would fight the dialog's own scrolling on short screens.
  dg.addEventListener('close', () => document.documentElement.classList.remove('dg-open'));

  const open = asset => {
    plat = /windows/.test(asset) ? 'win' : 'mac';
    step = 0;
    // the names the steps mention: this release's zip, and the folder it unpacks to
    const pick = { mac: /macos/.test(asset) ? asset : 'macos-arm64', win: 'windows-x64' };
    for (const p of ['mac', 'win']) {
      const name = ASSET_NAMES[pick[p]] || `loupe-${pick[p]}.zip`;
      $$(`.dg-zip[data-zip="${p}"]`, dg).forEach(e => { e.textContent = name; });
      $$(`.dg-dir[data-dir="${p}"]`, dg).forEach(e => { e.textContent = `loupe-${pick[p]}`; });
    }
    show();
    document.documentElement.classList.add('dg-open');
    if (!dg.open) dg.showModal();
    next.focus({ preventScroll: true });
  };
  $$('[data-downloads] .dl').forEach(b => b.addEventListener('click', e => {
    // Before the release is looked up the link is the releases page: open that beside the guide
    // rather than navigating away from it.
    if (!/\.zip$/i.test(b.href)) { e.preventDefault(); window.open(b.href, '_blank', 'noopener'); }
    open(b.dataset.asset);
  }));
  $$('[data-guide]').forEach(b => b.addEventListener('click', () => open(b.dataset.guide)));
}

// ---------------------------------------------------------------------------------------------
downloads();
firstRunGuide();
wayfinding();
ambientGlow();
if (motion) {
  gsap.registerPlugin(ScrollTrigger);
  measure();
  buildTimeline();
  bindWorkspaceJumps();
  rotator();
  addEventListener('resize', () => ScrollTrigger.refresh());
  document.fonts?.ready.then(() => ScrollTrigger.refresh());
  if (deepScene > 0) history.scrollRestoration = 'manual';
} else {
  document.documentElement.classList.remove('preload'); // no story: show the plain layout now
  S.spin = 0;
  measure();
  buildStills();
  addEventListener('resize', measure);
}
requestAnimationFrame(loop);
