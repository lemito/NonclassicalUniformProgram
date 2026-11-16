"use strict";

const PALETTE = ["#2f6fde", "#e8590c", "#2b8a3e", "#c2255c", "#7048e8", "#0b7285", "#e67700", "#5c940d", "#a61e4d", "#868e96"];
const colorOf = (c) => PALETTE[((c % PALETTE.length) + PALETTE.length) % PALETTE.length];
const fmt = (v, d = 3) => (Number.isFinite(v) ? v.toFixed(d) : "—");

let state = null;
let currentTab = "gen";
let lastOps = null;

const $ = (id) => document.getElementById(id);

function themeColors() {
  const s = getComputedStyle(document.documentElement);
  return {
    text: s.getPropertyValue("--text").trim(),
    muted: s.getPropertyValue("--muted").trim(),
    grid: s.getPropertyValue("--grid").trim(),
    axis: s.getPropertyValue("--axis").trim(),
    panel: s.getPropertyValue("--panel").trim(),
  };
}

function showStatus(msg, ok) {
  const el = $("status");
  el.textContent = msg;
  el.className = "status " + (ok ? "ok" : "error");
  clearTimeout(showStatus.t);
  showStatus.t = setTimeout(() => (el.className = "status"), ok ? 2500 : 6000);
}

async function api(path, body) {
  const opts = body === undefined ? {} : { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) };
  const res = await fetch(path, opts);
  const data = await res.json();
  if (!res.ok) throw new Error(data.error || res.statusText);
  return data;
}

function formData(form) {
  const out = {};
  for (const [k, v] of new FormData(form)) out[k] = v;
  return out;
}

function niceTicks(min, max, count) {
  const span = max - min;
  if (!(span > 0)) return [min];
  const raw = span / count;
  const mag = Math.pow(10, Math.floor(Math.log10(raw)));
  const norm = raw / mag;
  const step = (norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10) * mag;
  const ticks = [];
  for (let v = Math.ceil(min / step) * step; v <= max + step * 1e-9; v += step) ticks.push(+v.toFixed(10));
  return ticks;
}

function setupCanvas(cv) {
  if (!cv.dataset.h) cv.dataset.h = cv.getAttribute("height");
  const h = +cv.dataset.h;
  const w = cv.clientWidth || 600;
  const dpr = window.devicePixelRatio || 1;
  cv.style.height = h + "px";
  cv.width = Math.round(w * dpr);
  cv.height = Math.round(h * dpr);
  const ctx = cv.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);
  return { ctx, w, h };
}

function makePlot(cv, o) {
  const { ctx, w, h } = setupCanvas(cv);
  const th = themeColors();
  const pad = { l: 52, r: 16, t: 14, b: 38 };
  const pw = w - pad.l - pad.r;
  const ph = h - pad.t - pad.b;
  const X = (x) => pad.l + ((x - o.xmin) / (o.xmax - o.xmin)) * pw;
  const Y = (y) => pad.t + ph - ((y - o.ymin) / (o.ymax - o.ymin)) * ph;
  ctx.font = "11px system-ui, sans-serif";
  ctx.lineWidth = 1;
  ctx.strokeStyle = th.grid;
  ctx.fillStyle = th.muted;
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  for (const t of niceTicks(o.xmin, o.xmax, Math.max(3, Math.floor(pw / 80)))) {
    const x = X(t);
    ctx.beginPath();
    ctx.moveTo(x, pad.t);
    ctx.lineTo(x, pad.t + ph);
    ctx.stroke();
    ctx.fillText(String(t), x, pad.t + ph + 6);
  }
  if (o.yticks !== false) {
    ctx.textAlign = "right";
    ctx.textBaseline = "middle";
    for (const t of niceTicks(o.ymin, o.ymax, Math.max(3, Math.floor(ph / 50)))) {
      const y = Y(t);
      ctx.beginPath();
      ctx.moveTo(pad.l, y);
      ctx.lineTo(pad.l + pw, y);
      ctx.stroke();
      ctx.fillText(String(t), pad.l - 6, y);
    }
  }
  ctx.strokeStyle = th.axis;
  ctx.beginPath();
  ctx.moveTo(pad.l, pad.t);
  ctx.lineTo(pad.l, pad.t + ph);
  ctx.lineTo(pad.l + pw, pad.t + ph);
  ctx.stroke();
  ctx.fillStyle = th.muted;
  ctx.textAlign = "right";
  ctx.textBaseline = "bottom";
  if (o.xlabel) ctx.fillText(o.xlabel, pad.l + pw, h - 2);
  ctx.save();
  ctx.translate(12, pad.t + ph / 2);
  ctx.rotate(-Math.PI / 2);
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  if (o.ylabel) ctx.fillText(o.ylabel, 0, 0);
  ctx.restore();
  return { ctx, X, Y, pad, pw, ph, w, h, th };
}

function emptyCanvas(cv, text) {
  const { ctx, w, h } = setupCanvas(cv);
  ctx.fillStyle = themeColors().muted;
  ctx.font = "13px system-ui, sans-serif";
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText(text, w / 2, h / 2);
}

function xRange(items) {
  let lo = Infinity, hi = -Infinity;
  for (const it of items) {
    lo = Math.min(lo, it.a);
    hi = Math.max(hi, it.b);
  }
  const m = (hi - lo) * 0.03 || 1;
  return [lo - m, hi + m];
}

function drawCurves(cv, items, alpha, colorFn) {
  if (!items.length) return emptyCanvas(cv, "Набор пуст");
  const [xmin, xmax] = xRange(items);
  const p = makePlot(cv, { xmin, xmax, ymin: 0, ymax: 1.08, xlabel: "x", ylabel: "μ(x)" });
  const { ctx, X, Y } = p;
  const op = Math.max(0.12, Math.min(0.85, 25 / items.length));
  ctx.lineWidth = 1.2;
  ctx.globalAlpha = op;
  for (const it of items) {
    ctx.strokeStyle = colorFn(it);
    ctx.beginPath();
    ctx.moveTo(X(it.a), Y(0));
    ctx.lineTo(X(it.b), Y(1));
    ctx.lineTo(X(it.b), Y(0));
    ctx.stroke();
  }
  ctx.globalAlpha = Math.min(1, op * 2);
  ctx.lineWidth = 3;
  for (const it of items) {
    ctx.strokeStyle = colorFn(it);
    ctx.beginPath();
    ctx.moveTo(X(it.left), Y(alpha));
    ctx.lineTo(X(it.right), Y(alpha));
    ctx.stroke();
  }
  ctx.globalAlpha = 1;
  ctx.setLineDash([6, 4]);
  ctx.lineWidth = 1.5;
  ctx.strokeStyle = p.th.text;
  ctx.beginPath();
  ctx.moveTo(p.pad.l, Y(alpha));
  ctx.lineTo(p.pad.l + p.pw, Y(alpha));
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.fillStyle = p.th.text;
  ctx.textAlign = "left";
  ctx.textBaseline = "bottom";
  ctx.font = "12px system-ui, sans-serif";
  ctx.fillText("α = " + alpha, p.pad.l + 6, Y(alpha) - 3);
}

function drawIntervals(cv, rows, colorFn, opts = {}) {
  if (!rows.length) return emptyCanvas(cv, "Нет отрезков");
  let lo = Infinity, hi = -Infinity;
  for (const it of rows) {
    lo = Math.min(lo, it.left);
    hi = Math.max(hi, it.right);
  }
  const m = (hi - lo) * 0.03 || 1;
  const p = makePlot(cv, { xmin: lo - m, xmax: hi + m, ymin: 0, ymax: rows.length, yticks: false, xlabel: "x", ylabel: opts.ylabel || "отрезки" });
  const { ctx, X, Y } = p;
  const step = p.ph / rows.length;
  ctx.lineWidth = Math.max(1, Math.min(6, step * 0.7));
  ctx.lineCap = "butt";
  rows.forEach((it, i) => {
    const y = Y(rows.length - i - 0.5);
    const dim = opts.dimFn && opts.dimFn(it);
    ctx.globalAlpha = dim ? 0.25 : 1;
    ctx.strokeStyle = dim ? p.th.axis : colorFn(it);
    ctx.beginPath();
    ctx.moveTo(X(it.left), y);
    ctx.lineTo(Math.max(X(it.right), X(it.left) + 1), y);
    ctx.stroke();
  });
  ctx.globalAlpha = 1;
  if (opts.groupKey) {
    ctx.strokeStyle = p.th.axis;
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 3]);
    for (let i = 1; i < rows.length; i++) {
      if (opts.groupKey(rows[i]) !== opts.groupKey(rows[i - 1])) {
        const y = Y(rows.length - i);
        ctx.beginPath();
        ctx.moveTo(p.pad.l, y);
        ctx.lineTo(p.pad.l + p.pw, y);
        ctx.stroke();
      }
    }
    ctx.setLineDash([]);
  }
}

function drawScatter(cv, items, colorFn, clusters) {
  if (!items.length) return emptyCanvas(cv, "Нет отрезков");
  let x0 = Infinity, x1 = -Infinity, y0 = Infinity, y1 = -Infinity;
  for (const it of items) {
    x0 = Math.min(x0, it.left); x1 = Math.max(x1, it.left);
    y0 = Math.min(y0, it.right); y1 = Math.max(y1, it.right);
  }
  const mx = (x1 - x0) * 0.05 || 1, my = (y1 - y0) * 0.05 || 1;
  const p = makePlot(cv, { xmin: x0 - mx, xmax: x1 + mx, ymin: y0 - my, ymax: y1 + my, xlabel: "A", ylabel: "B" });
  const { ctx, X, Y } = p;
  for (const it of items) {
    ctx.fillStyle = colorFn(it);
    ctx.beginPath();
    ctx.arc(X(it.left), Y(it.right), 3.5, 0, Math.PI * 2);
    ctx.fill();
  }
  clusters.forEach((c, i) => {
    if (!c.count) return;
    const x = X(c.left), y = Y(c.right);
    ctx.fillStyle = colorOf(i);
    ctx.strokeStyle = p.th.text;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.rect(x - 6, y - 6, 12, 12);
    ctx.fill();
    ctx.stroke();
  });
}

function cutsPolygon(ctx, X, Y, c) {
  const m = c.alphas.length;
  ctx.beginPath();
  ctx.moveTo(X(c.left[0]), Y(0));
  for (let i = 0; i < m; i++) ctx.lineTo(X(c.left[i]), Y(c.alphas[i]));
  for (let i = m - 1; i >= 0; i--) ctx.lineTo(X(c.right[i]), Y(c.alphas[i]));
  ctx.closePath();
}

function drawOps(cv, res, alpha) {
  const sets = [
    { c: res.x, color: PALETTE[0], name: "X" },
    { c: res.y, color: PALETTE[1], name: "Y" },
  ];
  if (res.result) sets.push({ c: res.result, color: PALETTE[2], name: "Результат" });
  let lo = Infinity, hi = -Infinity;
  for (const s of sets) {
    lo = Math.min(lo, s.c.left[0]);
    hi = Math.max(hi, s.c.right[0]);
  }
  const m = (hi - lo) * 0.04 || 1;
  const p = makePlot(cv, { xmin: lo - m, xmax: hi + m, ymin: 0, ymax: 1.08, xlabel: "x", ylabel: "μ(x)" });
  const { ctx, X, Y } = p;
  for (const s of sets) {
    cutsPolygon(ctx, X, Y, s.c);
    ctx.globalAlpha = 0.15;
    ctx.fillStyle = s.color;
    ctx.fill();
    ctx.globalAlpha = 1;
    ctx.strokeStyle = s.color;
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.setLineDash([2, 3]);
    ctx.beginPath();
    ctx.moveTo(X(s.c.centroid), Y(0));
    ctx.lineTo(X(s.c.centroid), Y(1.04));
    ctx.stroke();
    ctx.setLineDash([]);
  }
  ctx.setLineDash([6, 4]);
  ctx.strokeStyle = p.th.text;
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.moveTo(p.pad.l, Y(alpha));
  ctx.lineTo(p.pad.l + p.pw, Y(alpha));
  ctx.stroke();
  ctx.setLineDash([]);
  $("opsLegend").innerHTML = sets
    .map((s) => `<span><span class="swatch" style="background:${s.color}"></span>${s.name}</span>`)
    .join("") + `<span>пунктир по вертикали — центр тяжести; горизонталь — α = ${alpha}</span>`;
}

function itemsSortedByLeft(items) {
  return items.slice().sort((p, q) => p.left - q.left || p.right - q.right);
}

function renderGen() {
  const items = state.items;
  drawCurves($("curvesGen"), items, state.alpha, () => PALETTE[0]);
  drawIntervals($("intervalsGen"), itemsSortedByLeft(items), () => PALETTE[0]);
  $("genCount").textContent = `(N = ${items.length}, α = ${state.alpha})`;
  $("alphaInput").value = state.alpha;
  const rows = items
    .map((it) => `<tr class="${it.selected ? "" : "dim"}"><td>${it.id}</td><td>${fmt(it.a)}</td><td>${fmt(it.b)}</td><td>${fmt(it.left)}</td><td>${fmt(it.right)}</td><td>${fmt(it.right - it.left)}</td><td>${it.selected ? "да" : "нет"}</td><td>${it.cluster >= 0 ? it.cluster + 1 : "—"}</td></tr>`)
    .join("");
  $("itemsTable").innerHTML = `<thead><tr><th>№</th><th>a</th><th>b</th><th>A</th><th>B</th><th>B − A</th><th>отобран</th><th>кластер</th></tr></thead><tbody>${rows}</tbody>`;
}

function renderSel() {
  const names = { none: "без отбора", width: "длина отрезка", window: "окно" };
  $("selInfo").textContent = `Критерий: ${names[state.criterion] || state.criterion}. Отобрано ${state.selected} из ${state.items.length}.`;
  drawIntervals($("intervalsSel"), itemsSortedByLeft(state.items), () => PALETTE[0], { dimFn: (it) => !it.selected });
}

function renderClu() {
  const sel = state.items.filter((it) => it.selected);
  const clustered = state.clusters.length > 0;
  const cfn = (it) => (clustered && it.cluster >= 0 ? colorOf(it.cluster) : PALETTE[9]);
  drawScatter($("scatterClu"), sel, cfn, state.clusters);
  const rows = sel.slice().sort((p, q) => p.cluster - q.cluster || p.left - q.left);
  drawIntervals($("intervalsClu"), rows, cfn, { groupKey: (it) => it.cluster, ylabel: "отрезки по кластерам" });
  drawCurves($("curvesClu"), sel, state.alpha, cfn);
  if (!clustered) {
    $("clusterTable").innerHTML = `<tbody><tr><td class="muted">Кластеризация ещё не выполнена. Будут использованы ${sel.length} отобранных отрезков.</td></tr></tbody>`;
    return;
  }
  const method = state.method === "kmeans" ? `k-means++, итераций: ${state.iterations}` : "пороговый (single-linkage)";
  const body = state.clusters
    .map((c, i) => `<tr><td><span class="swatch" style="background:${colorOf(i)}"></span>${i + 1}</td><td>${c.count}</td><td>${fmt(c.left)}</td><td>${fmt(c.right)}</td><td>${fmt(c.right - c.left)}</td><td>${fmt(c.spread)}</td></tr>`)
    .join("");
  $("clusterTable").innerHTML = `<caption class="muted" style="text-align:left;padding-bottom:6px">Метод: ${method}; кластеров: ${state.clusters.length}; отрезков: ${sel.length}</caption><thead><tr><th>Кластер</th><th>Размер</th><th>Центр A</th><th>Центр B</th><th>Длина</th><th>Разброс (СКО)</th></tr></thead><tbody>${body}</tbody>`;
}

function renderOps() {
  if (!lastOps) {
    emptyCanvas($("opsCanvas"), "Задайте величины и нажмите «Вычислить»");
    return $("opsForm").requestSubmit();
  }
  drawOps($("opsCanvas"), lastOps.res, lastOps.alpha);
}

async function loadLog() {
  try {
    const res = await fetch("/api/log");
    const text = await res.text();
    const pre = $("logText");
    pre.textContent = text;
    pre.scrollTop = pre.scrollHeight;
  } catch (e) {
    showStatus(e.message, false);
  }
}

function render() {
  if (!state) return;
  if (currentTab === "gen") renderGen();
  if (currentTab === "sel") renderSel();
  if (currentTab === "clu") renderClu();
  if (currentTab === "ops") renderOps();
  if (currentTab === "log") loadLog();
}

function setState(s, msg) {
  state = s;
  render();
  if (msg) showStatus(msg, true);
}

async function run(fn) {
  try {
    await fn();
  } catch (e) {
    showStatus(e.message, false);
  }
}

$("tabs").addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (!b) return;
  currentTab = b.dataset.tab;
  document.querySelectorAll("#tabs button").forEach((x) => x.classList.toggle("active", x === b));
  document.querySelectorAll(".tab").forEach((x) => x.classList.toggle("active", x.id === currentTab));
  render();
});

$("genForm").addEventListener("submit", (e) => {
  e.preventDefault();
  const f = formData(e.target);
  run(async () => {
    const body = { n: +f.n, seed: +f.seed, lo: +f.lo, hi: +f.hi, wmin: +f.wmin, wmax: +f.wmax, alpha: +f.alpha };
    setState(await api("/api/generate", body), `Сгенерировано ${body.n} величин`);
  });
});

$("addForm").addEventListener("submit", (e) => {
  e.preventDefault();
  const f = formData(e.target);
  run(async () => setState(await api("/api/add", { a: +f.a, b: +f.b }), "Величина добавлена"));
});

$("clearBtn").addEventListener("click", () => run(async () => setState(await api("/api/clear", {}), "Набор очищен")));

$("alphaBtn").addEventListener("click", () =>
  run(async () => setState(await api("/api/alpha", { alpha: +$("alphaInput").value }), "α-срезы пересчитаны"))
);

$("importFile").addEventListener("change", (e) => {
  const file = e.target.files[0];
  if (!file) return;
  run(async () => {
    const res = await fetch("/api/import", { method: "POST", body: await file.text() });
    const data = await res.json();
    if (!res.ok) throw new Error(data.error);
    setState(data, `Импортировано ${data.items.length} величин`);
    e.target.value = "";
  });
});

$("selForm").addEventListener("submit", (e) => {
  e.preventDefault();
  const f = formData(e.target);
  run(async () => {
    const s = await api("/api/select", { criterion: f.criterion, p1: +f.p1, p2: +f.p2 });
    setState(s, `Отобрано ${s.selected} отрезков`);
  });
});

$("cluForm").addEventListener("submit", (e) => {
  e.preventDefault();
  const f = formData(e.target);
  run(async () => {
    const s = await api("/api/cluster", { method: f.method, k: +f.k, delta: +f.delta, seed: +f.seed });
    setState(s, `Получено кластеров: ${s.clusters.length}`);
  });
});

$("pickBtn").addEventListener("click", () => {
  const form = $("opsForm");
  const pick = (id, pa, pb) => {
    if ($(id).value === "") return;
    const it = state && state.items[+$(id).value];
    if (!it) throw new Error("Нет величины с номером " + $(id).value);
    form.elements[pa].value = it.a;
    form.elements[pb].value = it.b;
  };
  try {
    pick("pickX", "xa", "xb");
    pick("pickY", "ya", "yb");
  } catch (err) {
    showStatus(err.message, false);
  }
});

$("opsForm").addEventListener("submit", (e) => {
  e.preventDefault();
  const f = formData(e.target);
  run(async () => {
    const alpha = +f.alpha;
    const res = await api("/api/op", { x: { a: +f.xa, b: +f.xb }, y: { a: +f.ya, b: +f.yb }, op: f.op, alpha });
    lastOps = { res, alpha, op: f.op };
    renderOps();
    renderOpsResult(res, alpha, f.op);
    if (res.error) showStatus(res.error, false);
  });
});

function renderOpsResult(res, alpha, op) {
  const sym = { add: "+", sub: "−", mul: "×", div: "÷" }[op];
  const cmp = res.compare < 0 ? "X < Y" : res.compare > 0 ? "X > Y" : "X ≈ Y";
  let html = "<dl>";
  html += `<dt>X</dt><dd>носитель [${fmt(res.x.left[0])}, ${fmt(res.x.right[0])}], α-срез [${fmt(res.cutX[0])}, ${fmt(res.cutX[1])}], центр тяжести ${fmt(res.x.centroid)}</dd>`;
  html += `<dt>Y</dt><dd>носитель [${fmt(res.y.left[0])}, ${fmt(res.y.right[0])}], α-срез [${fmt(res.cutY[0])}, ${fmt(res.cutY[1])}], центр тяжести ${fmt(res.y.centroid)}</dd>`;
  html += `<dt>Сравнение</dt><dd>${cmp} (по центру тяжести); Poss(X ≥ Y) = ${fmt(res.possXgeY)}, Poss(Y ≥ X) = ${fmt(res.possYgeX)}</dd>`;
  if (res.result) {
    const r = res.result;
    const m = r.alphas.length - 1;
    html += `<dt>X ${sym} Y</dt><dd>носитель [${fmt(r.left[0])}, ${fmt(r.right[0])}], ядро [${fmt(r.left[m])}, ${fmt(r.right[m])}], α-срез [${fmt(res.cutResult[0])}, ${fmt(res.cutResult[1])}], центр тяжести ${fmt(r.centroid)}</dd>`;
  } else {
    html += `<dt>X ${sym} Y</dt><dd>${res.error}</dd>`;
  }
  html += "</dl>";
  if (res.result) {
    const r = res.result;
    const rows = r.alphas
      .map((a, i) => (i % 2 === 0 ? `<tr><td>${fmt(a, 2)}</td><td>${fmt(res.x.left[i])}</td><td>${fmt(res.x.right[i])}</td><td>${fmt(res.y.left[i])}</td><td>${fmt(res.y.right[i])}</td><td>${fmt(r.left[i])}</td><td>${fmt(r.right[i])}</td></tr>` : ""))
      .join("");
    html += `<div class="table-wrap" style="margin-top:12px"><table><thead><tr><th>α</th><th>X: L</th><th>X: R</th><th>Y: L</th><th>Y: R</th><th>Рез.: L</th><th>Рез.: R</th></tr></thead><tbody>${rows}</tbody></table></div>`;
  }
  $("opsResult").innerHTML = html;
}

$("logBtn").addEventListener("click", loadLog);

let resizeTimer;
window.addEventListener("resize", () => {
  clearTimeout(resizeTimer);
  resizeTimer = setTimeout(render, 150);
});
window.matchMedia("(prefers-color-scheme: dark)").addEventListener("change", render);

run(async () => {
  const s = await api("/api/state");
  if (s.items.length) return setState(s);
  $("genForm").requestSubmit();
});

if (location.hash) {
  const b = document.querySelector(`#tabs button[data-tab="${location.hash.slice(1)}"]`);
  if (b) b.click();
}
