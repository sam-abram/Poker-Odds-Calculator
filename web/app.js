"use strict";

// ---- constants -------------------------------------------------------------
const RANKS = ["A", "K", "Q", "J", "T", "9", "8", "7", "6", "5", "4", "3", "2"];
const SUITS = ["s", "h", "d", "c"];
const SUIT_GLYPH = { s: "♠", h: "♥", d: "♦", c: "♣" };
const CATS = [
  ["high", "High card"], ["pair", "Pair"], ["2pair", "Two pair"],
  ["trips", "Three of a kind"], ["straight", "Straight"], ["flush", "Flush"],
  ["full", "Full house"], ["quads", "Four of a kind"], ["sflush", "Straight flush"],
];

// ---- state -----------------------------------------------------------------
const state = {
  players: [emptyPlayer(), emptyPlayer()],
  board: [null, null, null, null, null],
  trials: 100000,
  data: null,        // last successful API response
  computing: false,
  picker: null,      // { type:'player'|'board', pi, ci } or { type:'board', bi }
};

function emptyPlayer() { return { cards: [null, null] }; }

// ---- helpers ---------------------------------------------------------------
const $ = (id) => document.getElementById(id);
const pct = (x) => (x * 100).toFixed(2);

function cardsInUse() {
  const set = new Set();
  for (const p of state.players) for (const c of p.cards) if (c) set.add(c);
  for (const c of state.board) if (c) set.add(c);
  return set;
}

function boardCards() { return state.board.filter(Boolean); }

function streetName(n) {
  return n === 0 ? "preflop" : n === 3 ? "flop" : n === 4 ? "turn" : n === 5 ? "river" : "board";
}

function ready() {
  if (state.players.some((p) => !p.cards[0] || !p.cards[1])) return false;
  const bc = boardCards().length;
  return bc === 0 || bc === 3 || bc === 4 || bc === 5;
}

// Seat positions as {x%, y%} evenly spaced around the felt oval, seat 0 at bottom.
function seatPositions(n) {
  const cx = 50, cy = 50, rx = 44, ry = 46;
  const out = [];
  for (let i = 0; i < n; i++) {
    const a = Math.PI / 2 + (i * 2 * Math.PI) / n; // start at bottom, go clockwise
    out.push({ x: cx + rx * Math.cos(a), y: cy + ry * Math.sin(a) });
  }
  return out;
}

// ---- card element ----------------------------------------------------------
function cardEl(card, target) {
  const el = document.createElement("div");
  el.className = "card";
  if (!card) {
    el.classList.add("empty");
    el.textContent = "+";
  } else {
    const rank = card[0], suit = card[1];
    el.classList.add("suit-" + suit);
    const r = document.createElement("span");
    r.className = "r";
    r.textContent = rank === "T" ? "10" : rank;
    const s = document.createElement("span");
    s.className = "s";
    s.textContent = SUIT_GLYPH[suit];
    el.append(r, s);
  }
  el.addEventListener("click", (e) => { e.stopPropagation(); openPicker(target, el); });
  el.addEventListener("contextmenu", (e) => { e.preventDefault(); clearSlot(target); });
  return el;
}

// ---- render ----------------------------------------------------------------
function render() {
  $("pcount").textContent = state.players.length;

  // board
  const board = $("board");
  board.innerHTML = "";
  for (let i = 0; i < 5; i++) board.appendChild(cardEl(state.board[i], { type: "board", bi: i }));
  $("street").textContent = streetName(boardCards().length);

  // per-player results (from the exact enumeration)
  const enumRes = state.data && state.data.results.find((r) => r.method === "enumeration");
  let leader = -1, best = -1;
  if (enumRes) enumRes.players.forEach((p, i) => { if (p.equity > best) { best = p.equity; leader = i; } });

  // seats
  const seats = $("seats");
  seats.innerHTML = "";
  const pos = seatPositions(state.players.length);
  state.players.forEach((pl, i) => {
    const seat = document.createElement("div");
    seat.className = "seat" + (i === leader ? " leader" : "");
    seat.style.left = pos[i].x + "%";
    seat.style.top = pos[i].y + "%";

    const hand = document.createElement("div");
    hand.className = "hand";
    hand.appendChild(cardEl(pl.cards[0], { type: "player", pi: i, ci: 0 }));
    hand.appendChild(cardEl(pl.cards[1], { type: "player", pi: i, ci: 1 }));
    seat.appendChild(hand);

    const stats = document.createElement("div");
    stats.className = "stats";
    const ps = enumRes ? enumRes.players[i] : null;
    if (ps) {
      stats.innerHTML =
        `<div class="plabel"><span class="who">P${i + 1}</span><span>${i === leader ? "leader" : ""}</span></div>` +
        `<div class="equity">${pct(ps.equity)}<small>% eq</small></div>` +
        `<div class="bar"><i style="width:${Math.max(1, ps.equity * 100).toFixed(1)}%"></i></div>` +
        `<div class="wl"><span>win <b>${pct(ps.win)}</b></span><span>tie <b>${pct(ps.tie)}</b></span></div>`;
      seat.addEventListener("mouseenter", () => showHover(i, seat));
      seat.addEventListener("mouseleave", hideHover);
    } else {
      stats.classList.add("empty");
      stats.textContent = pl.cards[0] && pl.cards[1] ? "P" + (i + 1) : "click + to add cards";
    }
    seat.appendChild(stats);
    seats.appendChild(seat);
  });

  renderCompare();
}

function renderStatus() {
  const el = $("status");
  el.classList.remove("err");
  if (state.computing) { el.textContent = "computing…"; return; }
  if (!ready()) {
    const bc = boardCards().length;
    if (state.players.some((p) => !p.cards[0] || !p.cards[1]))
      el.textContent = "Add two cards to every player to see the odds.";
    else el.textContent = `Deal ${bc < 3 ? "3 (flop)" : "one more"} board card${bc === 4 ? "" : "s"} — or clear to go preflop.`;
    return;
  }
  const enumRes = state.data && state.data.results.find((r) => r.method === "enumeration");
  if (enumRes)
    el.textContent = `Exact enumeration · ${enumRes.outcomes.toLocaleString()} boards · ${enumRes.seconds.toFixed(3)}s`;
}

function renderCompare() {
  const panel = $("compare");
  const mc = state.data && state.data.results.find((r) => r.method === "monte_carlo");
  const ex = state.data && state.data.results.find((r) => r.method === "enumeration");
  if (!mc || !ex) { panel.classList.add("hidden"); return; }

  let maxErr = 0;
  ex.players.forEach((p, i) => { maxErr = Math.max(maxErr, Math.abs(p.equity - mc.players[i].equity)); });
  const speed = mc.seconds > 0 ? (ex.seconds / mc.seconds) : 0;

  let rows = "";
  ex.players.forEach((p, i) => {
    const m = mc.players[i];
    rows += `<tr><td>P${i + 1}</td><td>${pct(p.equity)}</td>` +
            `<td>${pct(m.equity)} &plusmn;${pct(m.equity_ci)}</td>` +
            `<td>${(Math.abs(p.equity - m.equity) * 100).toFixed(2)}</td></tr>`;
  });

  panel.innerHTML =
    `<h3>Monte Carlo vs. full enumeration <span style="color:var(--muted);font-weight:400">(preflop)</span></h3>` +
    `<div class="sub">${mc.outcomes.toLocaleString()} random trials in <b>${mc.seconds.toFixed(3)}s</b> ` +
    `vs. ${ex.outcomes.toLocaleString()} exact boards in <b>${ex.seconds.toFixed(3)}s</b> ` +
    `&mdash; Monte Carlo was <b>${speed.toFixed(1)}&times;</b> faster, max equity error <b>${(maxErr * 100).toFixed(2)} pts</b>.</div>` +
    `<table><thead><tr><th>Player</th><th>Exact eq%</th><th>Monte Carlo eq%</th><th>&#916; pts</th></tr></thead>` +
    `<tbody>${rows}</tbody></table>`;
  panel.classList.remove("hidden");
}

// ---- hover (category distribution) ----------------------------------------
function showHover(pi, seatEl) {
  const enumRes = state.data && state.data.results.find((r) => r.method === "enumeration");
  if (!enumRes) return;
  const cats = enumRes.players[pi].categories;
  const cur = enumRes.players[pi].current;

  let rows = "";
  for (const [key, label] of CATS) {
    const v = cats[key] || 0;
    rows += `<div class="crow${v < 0.0005 ? " zero" : ""}"><span class="clab">${label}</span>` +
            `<span class="cbar"><i style="width:${Math.max(0, v * 100).toFixed(1)}%"></i></span>` +
            `<span class="cval">${(v * 100).toFixed(1)}%</span></div>`;
  }
  const h = $("hover");
  h.innerHTML = `<h4>P${pi + 1} — final hand odds</h4>` +
    (cur ? `<div class="cur">now: <b>${cur.label}</b></div>` : "") + rows;
  h.classList.remove("hidden");

  const r = seatEl.getBoundingClientRect();
  let left = r.right + 10, top = r.top;
  const hw = 232, hh = h.offsetHeight;
  if (left + hw > window.innerWidth - 8) left = r.left - hw - 10;
  if (left < 8) left = 8;
  top = Math.min(Math.max(8, top), window.innerHeight - hh - 8);
  h.style.left = left + "px";
  h.style.top = top + "px";
}
function hideHover() { $("hover").classList.add("hidden"); }

// ---- picker ----------------------------------------------------------------
function openPicker(target, anchorEl) {
  state.picker = target;
  const used = cardsInUse();
  const current = slotValue(target);
  const p = $("picker");
  p.innerHTML = "";

  for (const suit of SUITS) {
    const row = document.createElement("div");
    row.className = "prow";
    const lab = document.createElement("span");
    lab.className = "suitlab suit-" + suit;
    lab.textContent = SUIT_GLYPH[suit];
    row.appendChild(lab);
    for (const rank of RANKS) {
      const card = rank + suit;
      const cell = document.createElement("div");
      cell.className = "pc " + suit;
      cell.textContent = rank === "T" ? "10" : rank;
      if (used.has(card) && card !== current) cell.classList.add("used");
      else cell.addEventListener("click", (e) => { e.stopPropagation(); assign(target, card); });
      row.appendChild(cell);
    }
    p.appendChild(row);
  }
  const tools = document.createElement("div");
  tools.className = "ptools";
  const clr = document.createElement("button");
  clr.textContent = "Clear slot";
  clr.addEventListener("click", (e) => { e.stopPropagation(); clearSlot(target); closePicker(); });
  const cls = document.createElement("button");
  cls.textContent = "Close";
  cls.addEventListener("click", (e) => { e.stopPropagation(); closePicker(); });
  tools.append(clr, cls);
  p.appendChild(tools);

  p.classList.remove("hidden");
  const r = anchorEl.getBoundingClientRect();
  let left = r.left, top = r.bottom + 6;
  const pw = p.offsetWidth, ph = p.offsetHeight;
  if (left + pw > window.innerWidth - 8) left = window.innerWidth - pw - 8;
  if (top + ph > window.innerHeight - 8) top = r.top - ph - 6;
  p.style.left = Math.max(8, left) + "px";
  p.style.top = Math.max(8, top) + "px";
}
function closePicker() { state.picker = null; $("picker").classList.add("hidden"); }

function slotValue(t) {
  return t.type === "player" ? state.players[t.pi].cards[t.ci] : state.board[t.bi];
}
function setSlot(t, card) {
  if (t.type === "player") state.players[t.pi].cards[t.ci] = card;
  else state.board[t.bi] = card;
}
function assign(target, card) {
  setSlot(target, card);
  closePicker();
  onChange();
}
function clearSlot(target) {
  setSlot(target, null);
  onChange();
}

// ---- compute ---------------------------------------------------------------
let timer = null;
function onChange() {
  render();
  renderStatus();
  clearTimeout(timer);
  timer = setTimeout(compute, 150);
}

async function compute() {
  if (!ready()) { state.data = null; render(); renderStatus(); return; }
  const q = new URLSearchParams();
  q.set("players", state.players.map((p) => p.cards.join("")).join(","));
  const b = boardCards().join("");
  if (b) q.set("board", b);
  q.set("trials", String(state.trials));
  state.computing = true; renderStatus();
  try {
    const res = await fetch("/api/equity?" + q.toString());
    const data = await res.json();
    state.computing = false;
    if (data.error) {
      state.data = null; render();
      const el = $("status"); el.classList.add("err"); el.textContent = "Error: " + data.error;
      return;
    }
    state.data = data;
    render();
    renderStatus();
  } catch (e) {
    state.computing = false;
    state.data = null; render();
    const el = $("status"); el.classList.add("err"); el.textContent = "Server unreachable.";
  }
}

// ---- controls --------------------------------------------------------------
function addPlayer() {
  if (state.players.length >= 9) return;
  state.players.push(emptyPlayer());
  onChange();
}
function removePlayer() {
  if (state.players.length <= 2) return;
  state.players.pop();
  onChange();
}
function newHand() {
  state.players.forEach((p) => { p.cards = [null, null]; });
  state.board = [null, null, null, null, null];
  state.data = null;
  onChange();
}

function wire() {
  $("inc").addEventListener("click", addPlayer);
  $("dec").addEventListener("click", removePlayer);
  $("newhand").addEventListener("click", newHand);
  $("trials").addEventListener("change", (e) => {
    const v = parseInt(e.target.value, 10);
    if (Number.isFinite(v) && v >= 1000) { state.trials = v; if (ready()) compute(); }
  });
  // click outside closes the picker
  document.addEventListener("click", () => { if (state.picker) closePicker(); });
  window.addEventListener("resize", render);
}

wire();
render();
renderStatus();
