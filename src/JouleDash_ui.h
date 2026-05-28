// ---------------------------------------------------------------------------
// JouleSuite for ESP32 / ESP8266 — JouleOTA · JouleSerial · JouleNet · JouleDash
// Author: Chinmoy Bhuyan
// Email:  dikibhuyan@gmail.com
// (c) 2026 — MIT License
// ---------------------------------------------------------------------------
//
// JouleDash dashboard SPA. Single self-contained file — no external CDN,
// no fonts to download, no fingerprintable assets. Renders all 16 widget
// types over a single WebSocket. Mobile-first, theme-aware (dark / light /
// auto), keyboard-navigable.
//
// Sizing notes (target the gzipped budget, not the raw):
//   raw   ~16 KB   raw HTML
//   gz    ~ 5 KB   what actually crosses the wire
// Both numbers are recomputed by tools/gzip_ui.py — keep an eye on them
// when adding to this file.
#pragma once
#include <Arduino.h>

namespace joule {
static const char DASH_UI_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"/>
<meta name="theme-color" content="#0b0d12"/>
<meta name="apple-mobile-web-app-capable" content="yes"/>
<title>JouleDash</title>
<style>
/* JouleDash palette — neutral near-black base, one indigo accent, semantic
 * dots. Cards have deeper inner surfaces with strong drop-shadow so the
 * grid reads as floating panels rather than flat blocks. */
:root{
  /* surfaces */
  --bg:#08090f; --panel:#11131c; --panel-2:#161824; --panel-solid:#161824;
  --ink:#f1f3fa; --ink2:#b5bbd0; --muted:#7a809a; --line:rgba(255,255,255,.06);
  --ring:rgba(99,102,241,.18);
  /* brand — same family for any gradient companion */
  --brand:#6366f1; --brand-2:#8b5cf6;
  /* semantic — used only on status pills / dots, never the brand */
  --ok:#10b981; --warn:#f59e0b; --err:#ef4444; --info:#06b6d4;
  --r:20px;
  --shadow:0 1px 0 rgba(255,255,255,.04) inset, 0 24px 48px -16px rgba(0,0,0,.55), 0 4px 12px -4px rgba(0,0,0,.3);
  --shadow-hover:0 1px 0 rgba(255,255,255,.06) inset, 0 30px 60px -18px rgba(99,102,241,.35), 0 6px 16px -4px rgba(0,0,0,.35);
  --grad:linear-gradient(135deg,var(--brand) 0%,var(--brand-2) 100%);
}
:root[data-theme="light"]{
  --bg:#f6f7fb; --panel:#ffffff; --panel-2:#ffffff; --panel-solid:#fff;
  --ink:#0b0d18; --ink2:#42485c; --muted:#7d8398; --line:rgba(15,18,40,.08);
  --shadow:0 1px 0 rgba(255,255,255,.6) inset, 0 24px 48px -22px rgba(15,18,40,.18), 0 4px 12px -4px rgba(15,18,40,.05);
  --shadow-hover:0 1px 0 rgba(255,255,255,.7) inset, 0 30px 60px -24px rgba(99,102,241,.22), 0 6px 14px -3px rgba(15,18,40,.08);
}
@media(prefers-color-scheme:light){:root[data-theme="auto"]{
  --bg:#f6f7fb; --panel:#ffffff; --panel-2:#ffffff; --panel-solid:#fff;
  --ink:#0b0d18; --ink2:#42485c; --muted:#7d8398; --line:rgba(15,18,40,.08);
  --shadow:0 1px 0 rgba(255,255,255,.6) inset, 0 24px 48px -22px rgba(15,18,40,.18), 0 4px 12px -4px rgba(15,18,40,.05);
  --shadow-hover:0 1px 0 rgba(255,255,255,.7) inset, 0 30px 60px -24px rgba(99,102,241,.22), 0 6px 14px -3px rgba(15,18,40,.08);
}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;height:100%}
body{
  font:14.5px/1.5 -apple-system,BlinkMacSystemFont,"Inter","SF Pro Display","Segoe UI",Roboto,sans-serif;
  font-feature-settings:"cv11","ss01","ss02";
  color:var(--ink);background:var(--bg);
  background-image:
    radial-gradient(900px 600px at 12% -8%,color-mix(in srgb,var(--brand) 22%,transparent),transparent 55%),
    radial-gradient(700px 480px at 92% 6%,color-mix(in srgb,var(--brand-2) 18%,transparent),transparent 55%),
    radial-gradient(900px 700px at 50% 120%,color-mix(in srgb,var(--brand) 8%,transparent),transparent 60%);
  background-attachment:fixed;min-height:100vh;
}
header{
  position:sticky;top:0;z-index:10;backdrop-filter:saturate(160%) blur(20px);
  -webkit-backdrop-filter:saturate(160%) blur(20px);
  background:color-mix(in srgb,var(--bg) 75%,transparent);
  border-bottom:1px solid var(--line);padding:14px 22px;display:flex;align-items:center;gap:14px;
}
.logo{width:36px;height:36px;border-radius:11px;background:var(--grad);
  display:grid;place-items:center;color:#fff;font-weight:800;font-size:15px;
  box-shadow:0 8px 24px -4px color-mix(in srgb,var(--brand) 60%,transparent),
             inset 0 1px 0 rgba(255,255,255,.25);}
.title{font-weight:600;font-size:15.5px;letter-spacing:-.2px}
.sub{font-size:11.5px;color:var(--muted);font-weight:500;letter-spacing:.1px}
.spacer{flex:1}
.pill{display:inline-flex;align-items:center;gap:7px;padding:6px 11px;border-radius:99px;
  background:color-mix(in srgb,var(--ink) 4%,transparent);border:1px solid var(--line);
  font-size:12px;color:var(--ink2);font-weight:500;font-variant-numeric:tabular-nums}
.dot{width:6px;height:6px;border-radius:50%;background:var(--muted)}
.dot.on{background:var(--ok);box-shadow:0 0 0 4px color-mix(in srgb,var(--ok) 18%,transparent);
  animation:pulse 2.4s ease-in-out infinite}
@keyframes pulse{0%,100%{box-shadow:0 0 0 4px color-mix(in srgb,var(--ok) 18%,transparent)}
                  50%{box-shadow:0 0 0 7px color-mix(in srgb,var(--ok) 6%,transparent)}}
.iconbtn{width:36px;height:36px;border-radius:11px;border:1px solid var(--line);
  background:color-mix(in srgb,var(--ink) 4%,transparent);color:var(--ink);
  display:grid;place-items:center;cursor:pointer;transition:.2s}
.iconbtn:hover{border-color:color-mix(in srgb,var(--brand) 50%,var(--line));
  color:var(--brand);transform:translateY(-1px)}
.tabs{display:flex;gap:4px;padding:14px 22px 8px;overflow-x:auto;scrollbar-width:none;
  position:sticky;top:65px;z-index:9;background:color-mix(in srgb,var(--bg) 75%,transparent);
  backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px)}
.tabs::-webkit-scrollbar{display:none}
.tab{padding:8px 14px;border:1px solid transparent;background:transparent;color:var(--muted);
  border-radius:99px;cursor:pointer;font:inherit;font-size:13px;font-weight:500;
  white-space:nowrap;transition:.2s;letter-spacing:.1px}
.tab:hover{color:var(--ink2);background:color-mix(in srgb,var(--ink) 4%,transparent)}
.tab.active{background:color-mix(in srgb,var(--ink) 6%,transparent);color:var(--ink);
  border-color:var(--line);font-weight:600}
main{padding:14px 22px 60px}
.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:14px}
.card{
  background:var(--panel);border:1px solid var(--line);border-radius:var(--r);
  padding:20px;display:flex;flex-direction:column;gap:10px;min-height:120px;
  position:relative;overflow:hidden;transition:transform .25s cubic-bezier(.3,.8,.2,1),
    box-shadow .25s ease, border-color .2s ease;
  box-shadow:var(--shadow);
}
.card:hover{transform:translateY(-2px);
  border-color:color-mix(in srgb,var(--brand) 30%,var(--line));
  box-shadow:var(--shadow-hover)}
.card::before{content:"";position:absolute;inset:0 0 auto 0;height:50%;border-radius:inherit;
  pointer-events:none;background:linear-gradient(180deg,rgba(255,255,255,.025),transparent 80%)}
/* Card accent strip — left border in semantic colour, matches widget purpose */
.card.c-success::after{content:"";position:absolute;left:0;top:14px;bottom:14px;width:3px;border-radius:3px;background:var(--ok)}
.card.c-warning::after{content:"";position:absolute;left:0;top:14px;bottom:14px;width:3px;border-radius:3px;background:var(--warn)}
.card.c-danger::after {content:"";position:absolute;left:0;top:14px;bottom:14px;width:3px;border-radius:3px;background:var(--err)}
.card.c-info::after   {content:"";position:absolute;left:0;top:14px;bottom:14px;width:3px;border-radius:3px;background:var(--info)}
.card.c-primary::after{content:"";position:absolute;left:0;top:14px;bottom:14px;width:3px;border-radius:3px;background:var(--brand)}
.lbl{font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:1px;font-weight:600}
/* Big numeric readout — light weight, big size, OpenType tabular-nums so the
 * value doesn't jiggle as digits change. Native Inter / SF Pro on the system. */
.val{font-size:32px;font-weight:300;line-height:1.05;font-variant-numeric:tabular-nums;
  color:var(--ink);letter-spacing:-1px;font-feature-settings:"tnum","ss01"}
.unit{font-size:13px;color:var(--muted);margin-left:6px;font-weight:500;letter-spacing:0;
  vertical-align:.18em}
.btn{padding:11px 18px;border-radius:12px;border:0;background:var(--grad);color:#fff;
  font:inherit;font-weight:600;cursor:pointer;transition:.2s cubic-bezier(.3,.8,.2,1);
  min-height:42px;letter-spacing:-.1px;font-size:13.5px;
  box-shadow:0 8px 24px -8px color-mix(in srgb,var(--brand) 80%,transparent),
             inset 0 1px 0 rgba(255,255,255,.2)}
.btn:hover{transform:translateY(-2px);
  box-shadow:0 14px 28px -10px color-mix(in srgb,var(--brand) 90%,transparent),
             inset 0 1px 0 rgba(255,255,255,.25)}
.btn:active{transform:translateY(0)}
.btn.ghost{background:color-mix(in srgb,var(--ink) 5%,transparent);color:var(--ink);
  border:1px solid var(--line);box-shadow:none}
.btn.ghost:hover{border-color:color-mix(in srgb,var(--brand) 50%,var(--line));color:var(--brand)}
.btn.danger{background:linear-gradient(135deg,var(--err),#f87171);
  box-shadow:0 8px 24px -8px color-mix(in srgb,var(--err) 70%,transparent),
             inset 0 1px 0 rgba(255,255,255,.2)}
.toggle{position:relative;width:48px;height:28px;background:color-mix(in srgb,var(--ink) 12%,transparent);
  border-radius:99px;cursor:pointer;align-self:flex-start;transition:.25s ease;
  border:1px solid transparent}
.toggle::after{content:"";position:absolute;left:3px;top:3px;width:20px;height:20px;
  background:#fff;border-radius:50%;transition:.28s cubic-bezier(.34,1.56,.64,1);
  box-shadow:0 2px 6px rgba(0,0,0,.3),inset 0 1px 0 rgba(255,255,255,.6)}
.toggle.on{background:var(--grad);box-shadow:0 4px 14px -4px color-mix(in srgb,var(--brand) 70%,transparent)}
.toggle.on::after{left:23px}
.slider-wrap{position:relative;padding-top:18px}
input[type=range]{width:100%;-webkit-appearance:none;appearance:none;background:transparent;
  height:24px;outline:none}
input[type=range]::-webkit-slider-runnable-track{height:6px;border-radius:6px;background:var(--line)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;
  background:var(--grad);margin-top:-7px;cursor:pointer;border:2px solid #fff;
  box-shadow:0 2px 8px color-mix(in srgb,var(--brand) 35%,transparent)}
input[type=range]::-moz-range-track{height:6px;border-radius:6px;background:var(--line)}
input[type=range]::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:var(--brand);
  border:2px solid #fff;cursor:pointer}
.slider-val{position:absolute;right:0;top:0;font-size:13px;font-weight:700;color:var(--brand);
  font-family:"SF Mono",ui-monospace,Menlo,monospace}
input[type=text],input[type=number],input[type=color]{width:100%;padding:10px 12px;border-radius:10px;
  border:1px solid var(--line);background:var(--panel);color:var(--ink);font:inherit;font-size:14px;
  outline:none;transition:.15s;min-height:42px}
input[type=text]:focus,input[type=number]:focus{border-color:var(--brand);
  box-shadow:0 0 0 3px color-mix(in srgb,var(--brand) 22%,transparent)}
.gauge{position:relative;height:96px}
.gauge svg{width:100%;height:100%}
.gauge .gv{position:absolute;left:0;right:0;bottom:0;text-align:center;font-weight:700;
  font-size:18px;font-family:"SF Mono",ui-monospace,Menlo,monospace;color:var(--ink)}
.progress{height:12px;background:var(--line);border-radius:99px;overflow:hidden;position:relative}
.progress>span{display:block;height:100%;width:0;background:var(--grad);border-radius:99px;
  transition:width .35s cubic-bezier(.3,.8,.2,1);
  box-shadow:0 0 12px color-mix(in srgb,var(--brand) 50%,transparent)}
.status{display:flex;align-items:center;gap:10px;font-weight:700;font-size:15px}
.status .pill-led{width:10px;height:10px;border-radius:50%;background:var(--muted);
  box-shadow:0 0 0 3px color-mix(in srgb,currentColor 18%,transparent)}
.status.ok .pill-led{background:var(--ok);color:var(--ok)}
.status.warn .pill-led{background:var(--warn);color:var(--warn)}
.status.err .pill-led{background:var(--err);color:var(--err)}
.chart canvas{width:100%;height:140px;display:block}
.donut{position:relative;display:grid;place-items:center;height:140px}
.donut svg{width:120px;height:120px}
.donut .dv{position:absolute;font-weight:800;font-size:22px;font-family:"SF Mono",monospace}
.img img{max-width:100%;border-radius:10px;display:block}
.joystick{width:140px;height:140px;border-radius:50%;align-self:center;cursor:pointer;
  background:radial-gradient(circle at 30% 30%,color-mix(in srgb,var(--brand) 14%,var(--panel)),var(--panel));
  border:1px solid var(--line);position:relative;touch-action:none}
.joystick::after{content:"";position:absolute;inset:14px;border-radius:50%;border:1px dashed var(--line);opacity:.6}
.joystick .nub{width:44px;height:44px;border-radius:50%;background:var(--grad);
  position:absolute;left:48px;top:48px;transition:transform .08s linear;
  box-shadow:0 6px 16px color-mix(in srgb,var(--brand) 35%,transparent),inset 0 2px 4px rgba(255,255,255,.3)}
.color-row{display:flex;gap:8px;align-items:center}
.color-row input[type=color]{width:56px;height:42px;padding:0;border:0;border-radius:10px;cursor:pointer;overflow:hidden}
.toast-wrap{position:fixed;right:14px;bottom:14px;display:flex;flex-direction:column;gap:8px;z-index:50;
  pointer-events:none;max-width:min(360px,90vw)}
.toast{background:var(--panel-solid);border:1px solid var(--line);padding:12px 16px;border-radius:12px;
  font-size:13px;animation:slideIn .3s ease;pointer-events:auto;box-shadow:var(--shadow);
  display:flex;align-items:center;gap:10px}
.toast::before{content:"";width:6px;height:32px;border-radius:3px;background:var(--info)}
.toast.success::before{background:var(--ok)}.toast.warn::before{background:var(--warn)}
.toast.error::before{background:var(--err)}
@keyframes slideIn{from{opacity:0;transform:translateX(20px)}to{opacity:1;transform:translateX(0)}}
.hidden{display:none!important}
@media(max-width:760px){
  .grid{grid-template-columns:repeat(6,1fr);gap:10px}
  main{padding:10px}
  .val{font-size:24px}
  header{padding:10px 14px}
  .pill{display:none}
}
@media(max-width:420px){.grid{grid-template-columns:repeat(2,1fr)}.card .val{font-size:22px}}
.glow{animation:glow 2s ease-in-out infinite alternate}
@keyframes glow{from{box-shadow:0 0 0 0 transparent}to{box-shadow:0 0 0 4px color-mix(in srgb,var(--brand) 18%,transparent)}}
</style></head><body>
<header>
  <div class="logo" aria-hidden="true">⚡</div>
  <div><div class="title" id="appTitle">JouleDash</div><div class="sub">live dashboard</div></div>
  <div class="spacer"></div>
  <span class="pill"><span class="dot" id="wsDot"></span><span id="wsTxt">connecting…</span></span>
  <button class="iconbtn" id="themeBtn" aria-label="toggle theme" title="theme">◐</button>
</header>
<nav class="tabs" id="tabs"></nav>
<main><div class="grid" id="grid"></div></main>
<div class="toast-wrap" id="toasts"></div>

<script>
const $=s=>document.querySelector(s);
let ws=null,layout=null,values={},currentTab=null;

function setStatus(on,t){const d=$("#wsDot");d.className="dot"+(on?" on":"");$("#wsTxt").textContent=t}
function open(){
  const proto=location.protocol==="https:"?"wss:":"ws:";
  ws=new WebSocket(proto+"//"+location.host+"/dash/ws");
  ws.onopen=()=>setStatus(true,"online");
  ws.onclose=()=>{setStatus(false,"offline · retry");setTimeout(open,1500)};
  ws.onerror=()=>setStatus(false,"error");
  ws.onmessage=ev=>{try{handle(JSON.parse(ev.data))}catch(e){console.warn(e)}};
}
function send(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o))}
function handle(m){
  if(m.type==="layout"){
    layout=m;
    applyTheme(m.theme);
    // Live-apply title + brand from the layout frame so setTitle() and
    // setBrandColor() from the host sketch take effect at runtime even
    // though the HTML itself ships pre-gzipped (no template substitution).
    if(m.title){document.title=m.title;const t=$("#appTitle");if(t)t.textContent=m.title}
    if(m.brand){document.documentElement.style.setProperty("--brand",m.brand);
                document.querySelector('meta[name="theme-color"]')?.setAttribute("content",m.brand)}
    renderTabs();render();
  }
  else if(m.type==="upd"){m.cards.forEach(c=>{values[c.id]=c.value;updateCard(c.id,c.value)})}
  else if(m.type==="notify"){toast(m.level,m.message,m.ttl)}
}
function applyTheme(t){
  // Default to dark for first-time visitors — it photographs better and is
  // closer to the typical embedded-device aesthetic. User toggle persists.
  const stored=localStorage.getItem("joule-theme");
  const mode=stored||t||"dark";
  document.documentElement.setAttribute("data-theme",mode);
}
$("#themeBtn").onclick=()=>{
  const cur=document.documentElement.getAttribute("data-theme")||"auto";
  const next=cur==="auto"?"dark":(cur==="dark"?"light":"auto");
  localStorage.setItem("joule-theme",next);applyTheme(next);toast("info","Theme: "+next,1500);
};

function renderTabs(){
  const t=$("#tabs"),list=layout.tabs&&layout.tabs.length?layout.tabs:["Main"];
  // Optional URL fragment: /dash#energy → preselect the matching tab on
  // first paint. Case-insensitive substring match so users can shorten.
  if(!currentTab){
    const hash=(location.hash||"").replace(/^#/,"").toLowerCase();
    if(hash){const m=list.find(x=>x.toLowerCase().includes(hash));if(m)currentTab=m;}
  }
  if(!currentTab||!list.includes(currentTab))currentTab=list[0];
  t.innerHTML=list.map(x=>`<button class="tab ${x===currentTab?"active":""}" data-t="${x}">${x}</button>`).join("");
  t.querySelectorAll(".tab").forEach(b=>b.onclick=()=>{
    currentTab=b.dataset.t;history.replaceState(null,"","#"+currentTab.toLowerCase());renderTabs();render();
  });
}

function render(){
  const g=$("#grid");g.innerHTML="";
  layout.cards.filter(c=>!c.hidden&&(c.tab||"Main")===currentTab).forEach(c=>{
    const e=document.createElement("div");
    const w=c.width||(c.type==="chart"?12:(c.type==="custom"?12:3));
    e.className="card c-"+(c.color||"default");
    e.style.gridColumn=`span ${Math.min(12,Math.max(1,w))}`;
    e.id="card-"+c.id;e.innerHTML=body(c);g.appendChild(e);wire(c,e);
    updateCard(c.id,values[c.id]??c.value);
  });
}
function body(c){
  const lbl=`<div class="lbl">${c.label||c.id}</div>`;
  switch(c.type){
    case"number":case"temperature":case"humidity":
      return `${lbl}<div class="val"><span id="o-${c.id}">—</span><span class="unit">${c.unit||""}</span></div>`;
    case"button":
      return `${lbl}<button class="btn" id="o-${c.id}">${c.label||"Press"}</button>`;
    case"switch":
      return `${lbl}<div class="toggle" id="o-${c.id}" role="switch" tabindex="0"></div>`;
    case"slider":
      return `${lbl}<div class="slider-wrap"><span class="slider-val" id="ov-${c.id}">—</span>
        <input type="range" id="o-${c.id}" min="${c.min||0}" max="${c.max||100}" step="${c.step||1}"/></div>`;
    case"gauge":
      return `${lbl}<div class="gauge"><svg viewBox="0 0 100 60" preserveAspectRatio="xMidYMax meet">
        <defs><linearGradient id="gg-${c.id}" x1="0" y1="0" x2="1" y2="0">
          <stop offset="0%" stop-color="var(--brand)"/><stop offset="100%" stop-color="var(--brand-2)"/></linearGradient></defs>
        <path d="M10,50 A40,40 0 0 1 90,50" fill="none" stroke="var(--line)" stroke-width="8" stroke-linecap="round"/>
        <path id="o-${c.id}" d="M10,50 A40,40 0 0 1 90,50" fill="none" stroke="url(#gg-${c.id})" stroke-width="8" stroke-linecap="round" stroke-dasharray="0 999"/>
        </svg><div class="gv" id="ov-${c.id}">—</div></div>`;
    case"progress":
      return `${lbl}<div class="progress"><span id="o-${c.id}"></span></div><div class="lbl" id="ov-${c.id}" style="text-transform:none;letter-spacing:0">—</div>`;
    case"status":
      return `${lbl}<div class="status" id="o-${c.id}"><span class="pill-led"></span><span class="txt">—</span></div>`;
    case"input":
      return `${lbl}<input type="text" id="o-${c.id}" placeholder="${c.unit||"type and press enter"}"/>`;
    case"joystick":
      return `${lbl}<div class="joystick" id="o-${c.id}"><div class="nub"></div></div><div class="lbl" id="ov-${c.id}" style="text-align:center">x=0 y=0</div>`;
    case"color":
      return `${lbl}<div class="color-row"><input type="color" id="o-${c.id}"/><span class="val" style="font-size:14px" id="ov-${c.id}">#000</span></div>`;
    case"image":
      return `${lbl}<div class="img"><img id="o-${c.id}" alt=""/></div>`;
    case"chart":
      return `${lbl}<div class="chart"><canvas id="o-${c.id}"></canvas></div>`;
    case"donut":
      return `${lbl}<div class="donut"><svg viewBox="0 0 36 36">
        <defs><linearGradient id="gg-${c.id}" x1="0" x2="1" y1="0" y2="1"><stop offset="0%" stop-color="var(--brand)"/><stop offset="100%" stop-color="var(--brand-2)"/></linearGradient></defs>
        <circle cx="18" cy="18" r="15.9" fill="none" stroke="var(--line)" stroke-width="3"/>
        <circle id="o-${c.id}" cx="18" cy="18" r="15.9" fill="none" stroke="url(#gg-${c.id})" stroke-width="3" stroke-linecap="round" stroke-dasharray="0 100" transform="rotate(-90 18 18)"/>
        </svg><div class="dv" id="ov-${c.id}">—</div></div>`;
    case"custom":
      return `${lbl}<div id="o-${c.id}">${c.custom||""}</div>`;
  }
  return lbl;
}
function wire(c,r){
  const o=r.querySelector("#o-"+c.id);if(!o)return;
  if(c.type==="button")o.onclick=()=>{send({type:"cmd",id:c.id,value:"1"});r.classList.add("glow");setTimeout(()=>r.classList.remove("glow"),600)};
  if(c.type==="switch"){o.onclick=()=>{o.classList.toggle("on");send({type:"cmd",id:c.id,value:o.classList.contains("on")?"1":"0"})};
    o.onkeydown=e=>{if(e.key===" "||e.key==="Enter"){e.preventDefault();o.click()}}}
  if(c.type==="slider")o.oninput=()=>{r.querySelector("#ov-"+c.id).textContent=o.value+(c.unit?" "+c.unit:"");send({type:"cmd",id:c.id,value:o.value})};
  if(c.type==="input")o.onchange=()=>send({type:"cmd",id:c.id,value:o.value});
  if(c.type==="color")o.oninput=()=>{r.querySelector("#ov-"+c.id).textContent=o.value;send({type:"cmd",id:c.id,value:o.value})};
  if(c.type==="joystick"){
    const n=o.querySelector(".nub");let drag=false;
    const m=(cx,cy)=>{
      const b=o.getBoundingClientRect(),dx=(cx-b.left-b.width/2)/(b.width/2),dy=(cy-b.top-b.height/2)/(b.height/2);
      const md=Math.min(1,Math.hypot(dx,dy)),ag=Math.atan2(dy,dx),x=Math.cos(ag)*md,y=Math.sin(ag)*md;
      n.style.transform=`translate(${x*48}px,${y*48}px)`;
      const xv=Math.round(x*100),yv=-Math.round(y*100);
      r.querySelector("#ov-"+c.id).textContent=`x=${xv} y=${yv}`;
      send({type:"cmd",id:c.id,value:`${xv},${yv}`});
    };
    o.onpointerdown=e=>{drag=true;o.setPointerCapture(e.pointerId);m(e.clientX,e.clientY)};
    o.onpointermove=e=>{if(drag)m(e.clientX,e.clientY)};
    o.onpointerup=()=>{drag=false;n.style.transform="translate(0,0)";r.querySelector("#ov-"+c.id).textContent="x=0 y=0";send({type:"cmd",id:c.id,value:"0,0"})};
  }
}
function updateCard(id,v){
  if(v==null)return;
  const c=layout.cards.find(x=>x.id===id);if(!c)return;
  const o=document.getElementById("o-"+id),ov=document.getElementById("ov-"+id);if(!o)return;
  switch(c.type){
    case"number":case"temperature":case"humidity":o.textContent=v;break;
    case"switch":o.classList.toggle("on",v==="1"||v===1||v===true);break;
    case"slider":if(document.activeElement!==o)o.value=v;if(ov)ov.textContent=v+(c.unit?" "+c.unit:"");break;
    case"progress":{
      const lo=c.min||0,hi=c.max||100,p=Math.max(0,Math.min(100,((parseFloat(v)-lo)/(hi-lo))*100));
      o.style.width=p+"%";if(ov)ov.textContent=v+(c.unit?" "+c.unit:"");break;
    }
    case"gauge":{
      const lo=c.min||0,hi=c.max||100,vv=Math.max(lo,Math.min(hi,parseFloat(v)||0));
      const p=(vv-lo)/(hi-lo),len=p*125.6;
      o.setAttribute("stroke-dasharray",`${len} 999`);
      if(ov)ov.textContent=vv.toFixed(1)+(c.unit?c.unit:"");break;
    }
    case"donut":{
      const lo=c.min||0,hi=c.max||100,p=Math.max(0,Math.min(100,((parseFloat(v)-lo)/(hi-lo))*100));
      o.setAttribute("stroke-dasharray",`${p} 100`);
      if(ov)ov.textContent=Math.round(p)+"%";break;
    }
    case"status":{
      const lv=String(v).toLowerCase(),k=/^(ok|online|connect)/.test(lv)?"ok":/warn/.test(lv)?"warn":/err|off|fail/.test(lv)?"err":"";
      o.className="status "+k;o.querySelector(".txt").textContent=v;break;
    }
    case"image":o.src=v.startsWith("http")||v.startsWith("data:")?v:("data:image/png;base64,"+v);break;
    case"color":o.value=v;if(ov)ov.textContent=v;break;
    case"input":if(document.activeElement!==o)o.value=v;break;
    case"chart":drawChart(o,v,c);break;
    case"custom":{const sp=o.querySelector(`#dash-${id}-out`);if(sp)sp.textContent=v;else o.dispatchEvent(new CustomEvent("joulevalue",{detail:v}));break}
  }
}
function drawChart(cv,v,c){
  const dpr=window.devicePixelRatio||1,w=cv.clientWidth,h=cv.clientHeight||140;
  cv.width=w*dpr;cv.height=h*dpr;const ctx=cv.getContext("2d");ctx.scale(dpr,dpr);ctx.clearRect(0,0,w,h);
  let xs=[],ys=[];try{const j=typeof v==="string"?JSON.parse(v):v;xs=j.x||[];ys=j.y||[]}catch(e){return}
  if(!ys.length)return;
  const mn=Math.min(...ys),mx=Math.max(...ys),rg=(mx-mn)||1;
  const css=getComputedStyle(document.documentElement);
  const b=css.getPropertyValue("--brand").trim()||"#7c5cff",b2=css.getPropertyValue("--brand-2").trim()||"#22d3ee";
  ctx.strokeStyle=css.getPropertyValue("--line").trim();ctx.lineWidth=1;
  for(let i=0;i<3;i++){const y=(i+1)*(h-12)/4+4;ctx.beginPath();ctx.moveTo(4,y);ctx.lineTo(w-4,y);ctx.stroke()}
  const g=ctx.createLinearGradient(0,0,w,0);g.addColorStop(0,b);g.addColorStop(1,b2);
  ctx.beginPath();ys.forEach((y,i)=>{const x=4+(i/(ys.length-1||1))*(w-8),yy=h-6-((y-mn)/rg)*(h-16);if(i)ctx.lineTo(x,yy);else ctx.moveTo(x,yy)});
  if(c.chartType==="area"||!c.chartType){ctx.lineTo(w-4,h-4);ctx.lineTo(4,h-4);ctx.closePath();
    const ag=ctx.createLinearGradient(0,0,0,h);ag.addColorStop(0,b+"55");ag.addColorStop(1,b+"00");
    ctx.fillStyle=ag;ctx.fill();}
  ctx.beginPath();ys.forEach((y,i)=>{const x=4+(i/(ys.length-1||1))*(w-8),yy=h-6-((y-mn)/rg)*(h-16);if(i)ctx.lineTo(x,yy);else ctx.moveTo(x,yy)});
  ctx.strokeStyle=g;ctx.lineWidth=2.4;ctx.lineJoin="round";ctx.stroke();
  const lx=4+(ys.length-1)/(ys.length-1||1)*(w-8),ly=h-6-((ys[ys.length-1]-mn)/rg)*(h-16);
  ctx.fillStyle=b;ctx.beginPath();ctx.arc(lx,ly,3.5,0,7);ctx.fill();
  ctx.fillStyle="#fff";ctx.beginPath();ctx.arc(lx,ly,1.5,0,7);ctx.fill();
}
function toast(level,msg,ttl){
  const w=$("#toasts"),t=document.createElement("div");
  t.className="toast "+(level||"info");t.textContent=msg;w.appendChild(t);
  setTimeout(()=>{t.style.opacity="0";t.style.transition="opacity .3s";setTimeout(()=>t.remove(),300)},ttl||4500);
}
open();
</script></body></html>)HTML";
} // namespace joule
