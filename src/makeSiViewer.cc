// makeSiViewer.cc
//
// SiDetector.root (Geant4 の出力) を読み込み、Si検出器アレイを3Dで表示する
// 単体の HTML ファイル (SiViewer.html) を生成する。
//
// 使い方 1: ROOT マクロとして実行
//   root -l -b -q 'makeSiViewer.cc("SiDetector.root","SiViewer.html")'
//
// 使い方 2: コンパイルして実行
//   g++ -O2 -std=c++17 makeSiViewer.cc -o makeSiViewer $(root-config --cflags --libs)
//   ./makeSiViewer SiDetector.root SiViewer.html
//
// 生成された HTML は外部ライブラリを使わず、データも HTML の中に埋め込まれる。
// そのため、ブラウザで開くだけで表示できる (ネットワーク不要)。
//
// 読み込む ntuple:
//   "hits"   : eventID, pixelID, face, ix, iy, edep [MeV]   (必須)
//   "events" : イベント総数の取得にだけ使う                   (なくてもよい)

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "TFile.h"
#include "TTree.h"
#include "TTreeReader.h"
#include "TTreeReaderValue.h"

// ---------------------------------------------------------------------------
// 検出器の形状。DetectorConstruction.cc の値と一致させること。
// ---------------------------------------------------------------------------
static const int kNPix = 8;  // 1面あたり 8 x 8 素子
static const double kPitch = 8.0;  // 素子のピッチ [mm]
static const double kPixSize = 7.5;  // 素子の1辺 [mm]
static const double kSensSize = 7.05;  // 有感部の1辺 [mm]
static const double kPixThick = 0.15;  // 素子の厚さ [mm]
static const double kBinWidth = 0.25;  // ヒストグラムのビン幅 [MeV]

// ---------------------------------------------------------------------------
// HTML テンプレート。/*DATA_JSON*/null の部分にデータを埋め込む。
// ---------------------------------------------------------------------------
static const char kHtml[] = R"HTML(<!DOCTYPE html>
<html lang="ja">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Si検出器アレイ ビューア</title>
<style>
:root{
  --bg:#ffffff; --fg:#1f2328; --muted:#5f6b7a; --panel:#f3f5f8; --border:#d0d7de;
  --cbg:#dfe3e8; --imp:#e8710a; --dec:#0d9488; --sel:rgba(70,110,255,.20);
}
@media (prefers-color-scheme: dark){
  :root{
    --bg:#101216; --fg:#e7e9ec; --muted:#9aa4b0; --panel:#181b21; --border:#2c323b;
    --cbg:#262b33; --sel:rgba(110,150,255,.28);
  }
}
*{box-sizing:border-box}
body{margin:0;padding:12px 16px;background:var(--bg);color:var(--fg);
  font:14px/1.55 system-ui,-apple-system,"Hiragino Sans","Yu Gothic UI","Noto Sans JP",sans-serif}
h1{font-size:18px;margin:0 0 2px}
.sub{color:var(--muted);font-size:12px;margin-bottom:10px}
.controls{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:8px;margin-bottom:10px}
fieldset{border:1px solid var(--border);border-radius:8px;padding:6px 10px 8px;background:var(--panel);margin:0}
legend{font-size:12px;color:var(--muted);padding:0 4px}
label{margin-right:10px;white-space:nowrap}
input[type=number]{width:86px}
button{cursor:pointer;margin:2px 4px 2px 0}
.main{display:grid;grid-template-columns:minmax(0,1.25fr) minmax(0,1fr);gap:12px}
@media (max-width:1000px){.main{grid-template-columns:1fr}}
.views{display:flex;gap:8px}
.vp{flex:1;min-width:0;border:1px solid var(--border);border-radius:8px;overflow:hidden}
canvas{display:block;width:100%}
.legend{display:flex;align-items:center;gap:8px;margin:8px 0;font-size:12px}
.swatch{width:18px;height:14px;border:1px solid var(--border);background:#fff}
.bar{flex:1;max-width:320px;height:14px;border:1px solid var(--border)}
.panel{border:1px solid var(--border);border-radius:8px;padding:8px;background:var(--panel)}
.stats{font-size:13px;margin-top:6px}
.note{font-size:12px;color:var(--muted);margin-top:8px}
#tip{position:fixed;display:none;pointer-events:none;background:rgba(20,22,28,.92);color:#fff;
  padding:5px 8px;border-radius:6px;font-size:12px;line-height:1.5;z-index:10;white-space:pre}
</style>
</head>
<body>
<h1>Si検出器アレイ ビューア</h1>
<div class="sub" id="info"></div>

<div class="controls">
  <fieldset>
    <legend>表示する対象</legend>
    <label><input type="radio" name="mode" value="imp">打ち込み</label>
    <label><input type="radio" name="mode" value="dec">崩壊</label>
    <label><input type="radio" name="mode" value="all" checked>同時（合算）</label>
    <label><input type="radio" name="mode" value="split">並べて表示</label>
  </fieldset>
  <fieldset>
    <legend>打ち込み／崩壊の境界エネルギー</legend>
    <input type="number" id="thr" min="0" step="1" value="100"> MeV 以上を「打ち込み」とする
  </fieldset>
  <fieldset>
    <legend>エネルギー範囲（検出したヒットの絞り込み）</legend>
    <input type="number" id="emin" min="0" step="0.25"> 〜
    <input type="number" id="emax" min="0" step="0.25"> MeV<br>
    <button id="btnAll">全範囲</button>
    <button id="btnDec">崩壊の帯</button>
    <button id="btnImp">打ち込みの帯</button>
    <button id="btnZoom">選択範囲を拡大</button>
  </fieldset>
  <fieldset>
    <legend>表示設定</legend>
    面：
    <label><input type="checkbox" class="face" value="0" checked>+z</label>
    <label><input type="checkbox" class="face" value="1" checked>+x</label>
    <label><input type="checkbox" class="face" value="2" checked>−x</label>
    <label><input type="checkbox" class="face" value="3" checked>+y</label>
    <label><input type="checkbox" class="face" value="4" checked>−y</label><br>
    <label>不透明度 <input type="range" id="alpha" min="15" max="100" value="100"></label>
    <label><input type="checkbox" id="logc">色を対数スケール</label>
    <label><input type="checkbox" id="logy" checked>ヒストグラムを対数軸</label><br>
    視点：
    <button data-view="in">入口から</button>
    <button data-view="obl">斜め</button>
    <button data-view="back">背面（外側）</button>
  </fieldset>
</div>

<div class="main">
  <div>
    <div class="views">
      <div class="vp" id="vp0"><canvas id="cv0"></canvas></div>
      <div class="vp" id="vp1" style="display:none"><canvas id="cv1"></canvas></div>
    </div>
    <div class="legend">
      <span class="swatch"></span><span>0（未検出）</span>
      <span>1</span><div class="bar" id="bar"></div><span id="lgmax">最大</span>
      <span id="lgnote"></span>
    </div>
    <div class="note">3D：ドラッグで回転／ホイールで拡大縮小／ダブルクリックで視点リセット／素子にマウスを載せると詳細を表示。
      色は「その素子がヒットを記録したイベント数」を表します（1イベントでも検出で青、多いほど赤）。</div>
  </div>
  <div class="panel">
    <canvas id="hist"></canvas>
    <div class="stats" id="stats"></div>
    <div class="note">ヒストグラム：ドラッグでエネルギー範囲を選択／ホイールで横軸の拡大縮小／縦の破線は打ち込み・崩壊の境界。
      縦軸は素子ごと・イベントごとのヒット数です。</div>
  </div>
</div>
<div class="note" id="caveat">
  ※ ROOT ファイルには粒子の種類と時刻が記録されていないため、「打ち込み」と「崩壊」はエネルギーで判定しています。
  1素子のエネルギーが境界エネルギー以上のヒットを「打ち込み」（イオンが止まった素子。同じ素子に入った崩壊のエネルギーも含む）、
  未満のヒットを「崩壊」（α線などが当たった素子）としています。
</div>
<div id="tip"></div>

<script>
(function(){
'use strict';
var DATA = /*DATA_JSON*/null;

var G = DATA.geom;
var EV = DATA.ev, PIX = DATA.pix, EN = DATA.e;
var N = PIX.length;
var NPIX = G.nPix, NPF = NPIX * NPIX, NFACE = 5, NTOT = NFACE * NPF;
var BW = DATA.binWidth;
var FACE_NAMES = ['+z（奥の壁）', '+x（側面）', '−x（側面）', '+y（側面）', '−y（側面）'];
var VH = 520;   // 3D表示の高さ [px]
var HH = 300;   // ヒストグラムの高さ [px]
var HL = 56, HR = 12, HT = 14, HB = 34;   // ヒストグラムの余白
var DEAD = '#b9bfc8';                     // 不感部の色

function $(id){ return document.getElementById(id); }
function clamp(v, a, b){ return Math.max(a, Math.min(b, v)); }
function css(name){ return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }

// ---- データの範囲 ----
var EMAX = 0, MAXEV = 0, i;
for (i = 0; i < N; i++) { if (EN[i] > EMAX) EMAX = EN[i]; if (EV[i] > MAXEV) MAXEV = EV[i]; }
if (!(EMAX > 0)) EMAX = 1;
EMAX = Math.ceil(EMAX / BW) * BW;
var NB = Math.round(EMAX / BW) + 1;

// ---- 状態 ----
var S = {
  mode: 'all', thr: 100, emin: 0, emax: EMAX, vmin: 0, vmax: EMAX,
  yaw: 190 * Math.PI / 180, pitch: 10 * Math.PI / 180, zoom: 1,
  faceOn: [true, true, true, true, true], alpha: 1, logc: false, logy: true
};
var YAW0 = S.yaw, PITCH0 = S.pitch;
var VIEWS = {'in': [190, 10], 'obl': [155, 22], 'back': [-25, 20]};

// ---- 素子の幾何 ----
function quad(c, a, b, h){
  var s = [[-1,-1],[1,-1],[1,1],[-1,1]], out = [], k;
  for (k = 0; k < 4; k++) {
    var p = s[k][0], q = s[k][1];
    out.push([c[0] + (a[0]*p + b[0]*q)*h, c[1] + (a[1]*p + b[1]*q)*h, c[2] + (a[2]*p + b[2]*q)*h]);
  }
  return out;
}
function buildElements(){
  var off = NPIX * G.pitch / 2 + G.thick / 2;
  var els = [], id;
  for (id = 0; id < NTOT; id++) {
    var face = Math.floor(id / NPF), r = id % NPF;
    var ix = Math.floor(r / NPIX), iy = r % NPIX;
    var u = (ix - (NPIX - 1) / 2) * G.pitch, v = (iy - (NPIX - 1) / 2) * G.pitch;
    var c, a, b;
    if (face === 0)      { c = [u, v, off];  a = [1,0,0]; b = [0,1,0]; }
    else if (face === 1) { c = [off, u, v];  a = [0,1,0]; b = [0,0,1]; }
    else if (face === 2) { c = [-off, u, v]; a = [0,1,0]; b = [0,0,1]; }
    else if (face === 3) { c = [u, off, v];  a = [1,0,0]; b = [0,0,1]; }
    else                 { c = [u, -off, v]; a = [1,0,0]; b = [0,0,1]; }
    els.push({id: id, face: face, ix: ix, iy: iy, c: c,
              ce: quad(c, a, b, G.size / 2), cs: quad(c, a, b, G.sens / 2)});
  }
  return els;
}
var ELS = buildElements();

// ---- 集計 ----
var C = {imp: new Int32Array(NTOT), dec: new Int32Array(NTOT)};
var EVSEEN = {imp: new Uint8Array(MAXEV + 1), dec: new Uint8Array(MAXEV + 1)};
var HIST_IMP = null, HIST_DEC = null;

function computeCounts(){
  C.imp.fill(0); C.dec.fill(0);
  EVSEEN.imp.fill(0); EVSEEN.dec.fill(0);
  var k;
  for (k = 0; k < N; k++) {
    var e = EN[k];
    if (e < S.emin || e > S.emax) continue;
    var p = PIX[k];
    if (p < 0 || p >= NTOT) continue;
    if (e >= S.thr) { C.imp[p]++; EVSEEN.imp[EV[k]] = 1; }
    else            { C.dec[p]++; EVSEEN.dec[EV[k]] = 1; }
  }
}
function buildHist(){
  HIST_IMP = new Float64Array(NB); HIST_DEC = new Float64Array(NB);
  var k;
  for (k = 0; k < N; k++) {
    var b = Math.min(NB - 1, Math.max(0, Math.floor(EN[k] / BW)));
    if (EN[k] >= S.thr) HIST_IMP[b]++; else HIST_DEC[b]++;
  }
}
function countFor(cat, id){
  if (cat === 'imp') return C.imp[id];
  if (cat === 'dec') return C.dec[id];
  return C.imp[id] + C.dec[id];
}
function maxCountFor(cats){
  var m = 0, id, j;
  for (id = 0; id < NTOT; id++) for (j = 0; j < cats.length; j++) {
    var v = countFor(cats[j], id);
    if (v > m) m = v;
  }
  return m;
}

// ---- 色 ----
function colorOf(n, max){
  if (n <= 0) return '#ffffff';
  var t = 0;
  if (max > 1) t = S.logc ? Math.log(n) / Math.log(max) : (n - 1) / (max - 1);
  t = clamp(t, 0, 1);
  return 'hsl(' + (240 * (1 - t)).toFixed(1) + ',90%,50%)';
}

// ---- キャンバス ----
function fit(cv, cssH){
  var dpr = window.devicePixelRatio || 1;
  var w = Math.max(120, Math.floor(cv.getBoundingClientRect().width));
  var pw = Math.round(w * dpr), ph = Math.round(cssH * dpr);
  if (cv.width !== pw || cv.height !== ph) { cv.width = pw; cv.height = ph; }
  cv.style.height = cssH + 'px';
  var ctx = cv.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return {ctx: ctx, w: w, h: cssH};
}
function path(ctx, pts){
  ctx.beginPath(); ctx.moveTo(pts[0][0], pts[0][1]);
  for (var k = 1; k < pts.length; k++) ctx.lineTo(pts[k][0], pts[k][1]);
  ctx.closePath();
}
function inPoly(p, x, y){
  var c = false, a, b;
  for (a = 0, b = p.length - 1; a < p.length; b = a++) {
    if (((p[a][1] > y) !== (p[b][1] > y)) &&
        (x < (p[b][0] - p[a][0]) * (y - p[a][1]) / (p[b][1] - p[a][1]) + p[a][0])) c = !c;
  }
  return c;
}

// ---- 3D ----
var panels = [
  {cv: $('cv0'), cat: 'all', label: '', hits: []},
  {cv: $('cv1'), cat: 'dec', label: '', hits: []}
];
function makeView(W, H){
  var cy = Math.cos(S.yaw), sy = Math.sin(S.yaw), cp = Math.cos(S.pitch), sp = Math.sin(S.pitch);
  var D = 300, F = 300, scale = Math.min(W, H) / 130 * S.zoom;
  function rot(p){
    var x1 = p[0]*cy + p[2]*sy, z1 = -p[0]*sy + p[2]*cy, y1 = p[1];
    return [x1, y1*cp - z1*sp, y1*sp + z1*cp];
  }
  function proj(p){
    var r = rot(p), k = F / (D - r[2]);
    return [W/2 + r[0]*k*scale, H/2 - r[1]*k*scale, r[2]];
  }
  return {rot: rot, proj: proj};
}
function drawPanel(P, max){
  var f = fit(P.cv, VH), ctx = f.ctx, W = f.w, H = f.h;
  ctx.globalAlpha = 1;
  ctx.fillStyle = css('--cbg'); ctx.fillRect(0, 0, W, H);
  var V = makeView(W, H), items = [], k;
  for (k = 0; k < ELS.length; k++) {
    if (!S.faceOn[ELS[k].face]) continue;
    items.push({el: ELS[k], z: V.rot(ELS[k].c)[2]});
  }
  items.sort(function(a, b){ return a.z - b.z; });
  P.hits = [];
  ctx.lineJoin = 'round';
  for (k = 0; k < items.length; k++) {
    var el = items[k].el;
    var pe = el.ce.map(V.proj), ps = el.cs.map(V.proj);
    var n = countFor(P.cat, el.id);
    ctx.globalAlpha = S.alpha;
    path(ctx, pe); ctx.fillStyle = DEAD; ctx.fill();
    path(ctx, ps); ctx.fillStyle = colorOf(n, max); ctx.fill();
    ctx.lineWidth = 0.6; ctx.strokeStyle = 'rgba(60,66,75,0.55)';
    path(ctx, pe); ctx.stroke();
    P.hits.push({el: el, pe: pe});
  }
  ctx.globalAlpha = 1;
  var fg = css('--fg');

  // ビームの矢印 (開放面 z = -32 mm の外側から入射)
  var half = NPIX * G.pitch / 2;
  var a = V.proj([0, 0, -half - 30]), b = V.proj([0, 0, -half - 2]);
  ctx.strokeStyle = fg; ctx.fillStyle = fg; ctx.lineWidth = 2;
  ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke();
  var ang = Math.atan2(b[1] - a[1], b[0] - a[0]);
  ctx.beginPath(); ctx.moveTo(b[0], b[1]);
  ctx.lineTo(b[0] - 9*Math.cos(ang - 0.4), b[1] - 9*Math.sin(ang - 0.4));
  ctx.lineTo(b[0] - 9*Math.cos(ang + 0.4), b[1] - 9*Math.sin(ang + 0.4));
  ctx.closePath(); ctx.fill();
  ctx.font = '12px system-ui, sans-serif';
  ctx.fillText('ビーム', a[0] + 6, a[1] - 6);

  // 座標軸
  var ax = [['x', [1,0,0], '#d1382b'], ['y', [0,1,0], '#2c9a3f'], ['z', [0,0,1], '#2b6fd6']];
  var ox = 46, oy = H - 40, q;
  for (q = 0; q < 3; q++) {
    var r = V.rot(ax[q][1]);
    ctx.strokeStyle = ax[q][2]; ctx.fillStyle = ax[q][2]; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(ox, oy); ctx.lineTo(ox + r[0]*28, oy - r[1]*28); ctx.stroke();
    ctx.fillText(ax[q][0], ox + r[0]*34 - 3, oy - r[1]*34 + 4);
  }

  ctx.fillStyle = fg; ctx.font = '13px system-ui, sans-serif';
  ctx.fillText(P.label, 10, 20);
}
function draw3D(){
  var split = S.mode === 'split';
  $('vp1').style.display = split ? 'block' : 'none';
  var lab = {
    imp: '打ち込み（' + S.thr + ' MeV 以上のヒット）',
    dec: '崩壊（' + S.thr + ' MeV 未満のヒット）',
    all: '同時表示（打ち込み＋崩壊）'
  };
  panels[0].cat = split ? 'imp' : S.mode;
  panels[1].cat = 'dec';
  panels[0].label = lab[panels[0].cat];
  panels[1].label = lab.dec;
  var max = maxCountFor(split ? ['imp', 'dec'] : [panels[0].cat]);
  drawPanel(panels[0], max);
  if (split) drawPanel(panels[1], max);
  updateLegend(max);
}
function updateLegend(max){
  var stops = [], k;
  for (k = 0; k <= 8; k++) stops.push('hsl(' + (240 * (1 - k / 8)) + ',90%,50%)');
  $('bar').style.background = 'linear-gradient(to right,' + stops.join(',') + ')';
  $('lgmax').textContent = max > 0 ? String(max) : '（該当なし）';
  $('lgnote').textContent = S.logc ? '（対数スケール）' : '（線形スケール）';
}

// ---- ツールチップ ----
var tip = $('tip');
function showTip(P, e){
  var r = P.cv.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top, h = null, k;
  for (k = P.hits.length - 1; k >= 0; k--) { if (inPoly(P.hits[k].pe, x, y)) { h = P.hits[k]; break; } }
  if (!h) { tip.style.display = 'none'; return; }
  var el = h.el;
  tip.textContent =
    'pixelID ' + el.id + '\n' + FACE_NAMES[el.face] + '  ix=' + el.ix + ', iy=' + el.iy + '\n' +
    '中心 (' + el.c.map(function(v){ return v.toFixed(1); }).join(', ') + ') mm\n' +
    '選択範囲内の検出数\n  打ち込み ' + C.imp[el.id] + ' ／ 崩壊 ' + C.dec[el.id];
  tip.style.left = (e.clientX + 14) + 'px';
  tip.style.top = (e.clientY + 14) + 'px';
  tip.style.display = 'block';
}

// ---- ヒストグラム ----
function niceStep(range, n){
  var raw = range / n, p = Math.pow(10, Math.floor(Math.log10(raw))), m = raw / p;
  return (m < 1.5 ? 1 : m < 3 ? 2 : m < 7 ? 5 : 10) * p;
}
function fmt(v){ return (Math.abs(v) >= 100 || v === Math.round(v)) ? String(Math.round(v * 100) / 100) : v.toFixed(2); }
function drawHist(){
  var cv = $('hist'), f = fit(cv, HH), ctx = f.ctx, W = f.w, H = f.h;
  var pw = W - HL - HR, ph = H - HT - HB;
  ctx.globalAlpha = 1;
  ctx.fillStyle = css('--cbg'); ctx.fillRect(0, 0, W, H);
  var range = S.vmax - S.vmin;
  var k = Math.max(1, Math.ceil(2 * range / (BW * pw)));
  var gw = k * BW;
  var g0 = Math.floor(S.vmin / gw), g1 = Math.ceil(S.vmax / gw);
  var bars = [], ymax = 1, g, j;
  for (g = g0; g < g1; g++) {
    var a = 0, b = 0, i0 = g * k;
    for (j = i0; j < i0 + k; j++) if (j >= 0 && j < NB) { a += HIST_IMP[j]; b += HIST_DEC[j]; }
    if (a > 0 || b > 0) { bars.push([g, a, b]); ymax = Math.max(ymax, a, b); }
  }
  var top = S.logy ? ymax * 1.5 : ymax * 1.08;
  var lo = Math.log10(0.5), hi = Math.log10(top);
  function yOf(c){
    if (c <= 0) return 0;
    var t = S.logy ? (Math.log10(c) - lo) / (hi - lo) : c / top;
    return Math.max(1, t * ph);
  }
  function xOf(e){ return HL + (e - S.vmin) / range * pw; }
  var fg = css('--fg'), muted = css('--muted'), cImp = css('--imp'), cDec = css('--dec');

  ctx.font = '11px system-ui, sans-serif';
  ctx.lineWidth = 1;
  // 縦軸の目盛り
  var yt = [], v;
  if (S.logy) { for (v = 1; v <= top; v *= 10) yt.push(v); }
  else { var ys = niceStep(top, 4); for (v = 0; v <= top; v += ys) yt.push(v); }
  ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
  for (j = 0; j < yt.length; j++) {
    var yy = HT + ph - (yt[j] === 0 ? 0 : yOf(yt[j]));
    ctx.strokeStyle = 'rgba(128,136,148,0.28)';
    ctx.beginPath(); ctx.moveTo(HL, yy); ctx.lineTo(HL + pw, yy); ctx.stroke();
    ctx.fillStyle = muted; ctx.fillText(fmt(yt[j]), HL - 6, yy);
  }
  // 横軸の目盛り
  var xs = niceStep(range, 7), x0 = Math.ceil(S.vmin / xs) * xs;
  ctx.textAlign = 'center'; ctx.textBaseline = 'top';
  for (v = x0; v <= S.vmax + 1e-9; v += xs) {
    var xx = xOf(v);
    ctx.strokeStyle = 'rgba(128,136,148,0.28)';
    ctx.beginPath(); ctx.moveTo(xx, HT); ctx.lineTo(xx, HT + ph); ctx.stroke();
    ctx.fillStyle = muted; ctx.fillText(fmt(v), xx, HT + ph + 5);
  }

  // 棒
  ctx.save();
  ctx.beginPath(); ctx.rect(HL, HT, pw, ph); ctx.clip();
  var sx0 = xOf(S.emin), sx1 = xOf(S.emax);
  var partial = (S.emin > 0 || S.emax < EMAX);
  if (partial) { ctx.fillStyle = css('--sel'); ctx.fillRect(sx0, HT, Math.max(1, sx1 - sx0), ph); }
  for (j = 0; j < bars.length; j++) {
    var bx0 = xOf(bars[j][0] * gw), bx1 = xOf((bars[j][0] + 1) * gw), bw = Math.max(1, bx1 - bx0 - 0.5);
    if (bars[j][2] > 0) { ctx.fillStyle = cDec; ctx.fillRect(bx0, HT + ph - yOf(bars[j][2]), bw, yOf(bars[j][2])); }
    if (bars[j][1] > 0) { ctx.fillStyle = cImp; ctx.fillRect(bx0, HT + ph - yOf(bars[j][1]), bw, yOf(bars[j][1])); }
  }
  // 選択範囲の境界線
  if (partial) {
    ctx.strokeStyle = 'rgba(70,110,255,0.9)'; ctx.lineWidth = 1.2;
    ctx.beginPath(); ctx.moveTo(sx0, HT); ctx.lineTo(sx0, HT + ph); ctx.moveTo(sx1, HT); ctx.lineTo(sx1, HT + ph); ctx.stroke();
  }
  // 打ち込み／崩壊の境界
  var tx = xOf(S.thr);
  ctx.setLineDash([5, 4]); ctx.strokeStyle = fg; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(tx, HT); ctx.lineTo(tx, HT + ph); ctx.stroke(); ctx.setLineDash([]);
  ctx.restore();

  // 枠・軸ラベル
  ctx.strokeStyle = muted; ctx.lineWidth = 1; ctx.strokeRect(HL, HT, pw, ph);
  ctx.fillStyle = fg; ctx.textAlign = 'center'; ctx.textBaseline = 'alphabetic';
  ctx.font = '12px system-ui, sans-serif';
  ctx.fillText('エネルギー [MeV]（素子ごとの検出エネルギー）', HL + pw / 2, H - 6);
  ctx.textAlign = 'right';
  ctx.fillStyle = cImp; ctx.fillText('■ 打ち込み', HL + pw - 78, HT + 14);
  ctx.fillStyle = cDec; ctx.fillText('■ 崩壊', HL + pw - 8, HT + 14);
  ctx.fillStyle = muted; ctx.textAlign = 'left';
  ctx.fillText('ビン幅 ' + fmt(gw) + ' MeV', HL + 6, HT + 14);
}

// ---- 統計表示 ----
function updateStats(){
  var nImp = 0, nDec = 0, det = 0, id, mx = 0, evI = 0, evD = 0, evA = 0, k;
  for (id = 0; id < NTOT; id++) {
    nImp += C.imp[id]; nDec += C.dec[id];
    var t = C.imp[id] + C.dec[id];
    if (t > 0) det++;
    if (t > mx) mx = t;
  }
  for (k = 0; k <= MAXEV; k++) {
    if (EVSEEN.imp[k]) evI++;
    if (EVSEEN.dec[k]) evD++;
    if (EVSEEN.imp[k] || EVSEEN.dec[k]) evA++;
  }
  var ne = DATA.nEvents;
  $('stats').innerHTML =
    '選択範囲 <b>' + fmt(S.emin) + '〜' + fmt(S.emax) + ' MeV</b><br>' +
    'ヒット数：打ち込み <b>' + nImp + '</b>／崩壊 <b>' + nDec + '</b>（全 ' + N + ' ヒット中）<br>' +
    '検出した素子：<b>' + det + '</b> / ' + NTOT + '（素子あたり最大 ' + mx + ' イベント）<br>' +
    'ヒットのあったイベント：<b>' + evA + '</b> / ' + ne +
    '（打ち込み ' + evI + '、崩壊 ' + evD + '）';
}

// ---- 更新 ----
var pendRefresh = false, pend3D = false;
function refresh(){ computeCounts(); draw3D(); drawHist(); updateStats(); }
function scheduleRefresh(){
  if (pendRefresh) return; pendRefresh = true;
  requestAnimationFrame(function(){ pendRefresh = false; refresh(); });
}
function scheduleDraw3D(){
  if (pend3D) return; pend3D = true;
  requestAnimationFrame(function(){ pend3D = false; draw3D(); });
}
function syncInputs(){
  $('emin').value = Math.round(S.emin * 1000) / 1000;
  $('emax').value = Math.round(S.emax * 1000) / 1000;
}

// ---- 3Dの操作 ----
panels.forEach(function(P){
  var cv = P.cv, drag = null;
  cv.addEventListener('pointerdown', function(e){
    drag = {x: e.clientX, y: e.clientY}; cv.setPointerCapture(e.pointerId); tip.style.display = 'none';
  });
  cv.addEventListener('pointermove', function(e){
    if (drag) {
      S.yaw += (e.clientX - drag.x) * 0.008;
      S.pitch = clamp(S.pitch + (e.clientY - drag.y) * 0.008, -1.55, 1.55);
      drag = {x: e.clientX, y: e.clientY};
      scheduleDraw3D();
    } else showTip(P, e);
  });
  cv.addEventListener('pointerup', function(){ drag = null; });
  cv.addEventListener('pointerleave', function(){ tip.style.display = 'none'; });
  cv.addEventListener('wheel', function(e){
    e.preventDefault();
    S.zoom = clamp(S.zoom * Math.exp(-e.deltaY * 0.0012), 0.4, 8);
    scheduleDraw3D();
  }, {passive: false});
  cv.addEventListener('dblclick', function(){ S.yaw = YAW0; S.pitch = PITCH0; S.zoom = 1; scheduleDraw3D(); });
});

// ---- ヒストグラムの操作 ----
(function(){
  var cv = $('hist'), drag = null;
  function energyAt(e){
    var r = cv.getBoundingClientRect();
    return S.vmin + (e.clientX - r.left - HL) / (r.width - HL - HR) * (S.vmax - S.vmin);
  }
  cv.addEventListener('pointerdown', function(e){
    drag = energyAt(e); cv.setPointerCapture(e.pointerId);
  });
  cv.addEventListener('pointermove', function(e){
    if (drag === null) return;
    var a = clamp(Math.min(drag, energyAt(e)), 0, EMAX), b = clamp(Math.max(drag, energyAt(e)), 0, EMAX);
    S.emin = a; S.emax = b; syncInputs(); scheduleRefresh();
  });
  cv.addEventListener('pointerup', function(){ drag = null; });
  cv.addEventListener('wheel', function(e){
    e.preventDefault();
    var c = energyAt(e), f = e.deltaY > 0 ? 1.25 : 0.8;
    var nr = clamp((S.vmax - S.vmin) * f, BW * 20, EMAX);
    var rel = (c - S.vmin) / (S.vmax - S.vmin);
    var nmin = clamp(c - rel * nr, 0, EMAX - nr);
    S.vmin = nmin; S.vmax = nmin + nr; scheduleRefresh();
  }, {passive: false});
})();

// ---- コントロール ----
Array.prototype.forEach.call(document.querySelectorAll('input[name=mode]'), function(el){
  el.addEventListener('change', function(){ if (el.checked) { S.mode = el.value; draw3D(); } });
});
$('thr').addEventListener('change', function(){
  S.thr = Math.max(0, parseFloat($('thr').value) || 0); buildHist(); refresh();
});
function onRange(){
  var a = parseFloat($('emin').value), b = parseFloat($('emax').value);
  if (isNaN(a)) a = 0; if (isNaN(b)) b = EMAX;
  if (a > b) { var t = a; a = b; b = t; }
  S.emin = clamp(a, 0, EMAX); S.emax = clamp(b, 0, EMAX); syncInputs(); refresh();
}
$('emin').addEventListener('change', onRange);
$('emax').addEventListener('change', onRange);
$('btnAll').addEventListener('click', function(){
  S.emin = 0; S.emax = EMAX; S.vmin = 0; S.vmax = EMAX; syncInputs(); refresh();
});
$('btnDec').addEventListener('click', function(){ S.emin = 0; S.emax = Math.min(S.thr, EMAX); syncInputs(); refresh(); });
$('btnImp').addEventListener('click', function(){ S.emin = Math.min(S.thr, EMAX); S.emax = EMAX; syncInputs(); refresh(); });
$('btnZoom').addEventListener('click', function(){
  S.vmin = S.emin; S.vmax = Math.min(EMAX, Math.max(S.emax, S.emin + BW * 20)); refresh();
});
Array.prototype.forEach.call(document.querySelectorAll('button[data-view]'), function(b){
  b.addEventListener('click', function(){
    var v = VIEWS[b.getAttribute('data-view')];
    S.yaw = v[0] * Math.PI / 180; S.pitch = v[1] * Math.PI / 180; S.zoom = 1; draw3D();
  });
});
Array.prototype.forEach.call(document.querySelectorAll('input.face'), function(el){
  el.addEventListener('change', function(){ S.faceOn[parseInt(el.value, 10)] = el.checked; draw3D(); });
});
$('alpha').addEventListener('input', function(){ S.alpha = parseInt($('alpha').value, 10) / 100; draw3D(); });
$('logc').addEventListener('change', function(){ S.logc = $('logc').checked; draw3D(); });
$('logy').addEventListener('change', function(){ S.logy = $('logy').checked; drawHist(); });
window.addEventListener('resize', function(){ draw3D(); drawHist(); });
if (window.matchMedia) {
  var mq = window.matchMedia('(prefers-color-scheme: dark)');
  if (mq.addEventListener) mq.addEventListener('change', function(){ draw3D(); drawHist(); });
}

// ---- 初期化 ----
$('info').textContent = 'ファイル: ' + DATA.file + '　／　イベント数: ' + DATA.nEvents +
  '　／　ヒット行数: ' + N + '　／　素子 ' + NPIX + '×' + NPIX + ' × 5面（素子 ' + G.size +
  ' mm、有感部 ' + G.sens + ' mm、ピッチ ' + G.pitch + ' mm）';
buildHist();
syncInputs();
refresh();
})();
</script>
</body>
</html>
)HTML";

// ---------------------------------------------------------------------------
// 補助関数
// ---------------------------------------------------------------------------
static std::string jsonEscape(const std::string& s)
{
  std::string out;
  for (char c : s) {
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    }
    else if (c == '\n') {
      out += "\\n";
    }
    else {
      out += c;
    }
  }
  return out;
}

// ntuple は最上位にあるはずだが、"ntuples/" ディレクトリの中にある場合にも対応する
static TTree* getTree(TFile* f, const char* name)
{
  TTree* t = dynamic_cast<TTree*>(f->Get(name));
  if (!t) t = dynamic_cast<TTree*>(f->Get((std::string("ntuples/") + name).c_str()));
  return t;
}

// ---------------------------------------------------------------------------
// 本体
// ---------------------------------------------------------------------------
int makeSiViewer(const char* inFile = "SiDetector.root", const char* outFile = "SiViewer.html")
{
  std::unique_ptr<TFile> f(TFile::Open(inFile, "READ"));
  if (!f || f->IsZombie()) {
    std::cerr << "[makeSiViewer] ファイルを開けません: " << inFile << std::endl;
    return 1;
  }

  TTree* hits = getTree(f.get(), "hits");
  if (!hits) {
    std::cerr << "[makeSiViewer] ntuple \"hits\" が見つかりません。" << std::endl;
    return 1;
  }

  // --- hits を読み込む ---
  std::vector<int> evID, pixID;
  std::vector<double> edep;
  {
    TTreeReader reader(hits);
    TTreeReaderValue<Int_t> rEvent(reader, "eventID");
    TTreeReaderValue<Int_t> rPixel(reader, "pixelID");
    TTreeReaderValue<Double_t> rEdep(reader, "edep");
    while (reader.Next()) {
      evID.push_back(*rEvent);
      pixID.push_back(*rPixel);
      edep.push_back(*rEdep);
    }
  }

  // --- イベント総数 ---
  long long nEvents = 0;
  if (TTree* ev = getTree(f.get(), "events")) {
    nEvents = ev->GetEntries();
  }
  else if (!evID.empty()) {
    nEvents = *std::max_element(evID.begin(), evID.end()) + 1;
  }

  // --- JSON を組み立てる ---
  std::ostringstream js;
  js << std::setprecision(7);
  js << "{\"file\":\"" << jsonEscape(inFile) << "\"";
  js << ",\"nEvents\":" << nEvents;
  js << ",\"binWidth\":" << kBinWidth;
  js << ",\"geom\":{\"nPix\":" << kNPix << ",\"pitch\":" << kPitch << ",\"size\":" << kPixSize
     << ",\"sens\":" << kSensSize << ",\"thick\":" << kPixThick << "}";
  js << ",\"ev\":[";
  for (size_t i = 0; i < evID.size(); ++i) js << (i ? "," : "") << evID[i];
  js << "],\"pix\":[";
  for (size_t i = 0; i < pixID.size(); ++i) js << (i ? "," : "") << pixID[i];
  js << "],\"e\":[";
  for (size_t i = 0; i < edep.size(); ++i) js << (i ? "," : "") << edep[i];
  js << "]}";

  // --- テンプレートに埋め込んで書き出す ---
  std::string html(kHtml);
  const std::string key = "/*DATA_JSON*/null";
  const size_t pos = html.find(key);
  if (pos == std::string::npos) {
    std::cerr << "[makeSiViewer] テンプレートにデータの挿入位置がありません。" << std::endl;
    return 1;
  }
  html.replace(pos, key.size(), js.str());

  std::ofstream out(outFile, std::ios::binary);
  if (!out) {
    std::cerr << "[makeSiViewer] 出力ファイルを作れません: " << outFile << std::endl;
    return 1;
  }
  out << html;
  out.close();

  std::cout << "[makeSiViewer] " << outFile << " を作成しました（イベント数 " << nEvents
            << "、ヒット行数 " << evID.size() << "）" << std::endl;
  return 0;
}

#if !defined(__CLING__)
int main(int argc, char** argv)
{
  const char* in = (argc > 1) ? argv[1] : "SiDetector.root";
  const char* out = (argc > 2) ? argv[2] : "SiViewer.html";
  return makeSiViewer(in, out);
}
#endif
