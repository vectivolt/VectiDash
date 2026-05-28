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
<title>__TITLE__</title>
<style>
:root{
  --bg:#0a0c12;--bg2:#0e1320;--panel:rgba(255,255,255,.04);--panel-solid:#141a2a;
  --ink:#e8ecf5;--ink2:#a5acc1;--muted:#6b7390;--line:rgba(255,255,255,.08);
  --brand:__BRAND__;--brand-2:#22d3ee;--ok:#3ddc97;--warn:#ffb347;--err:#ff6b81;--info:#7aa6ff;
  --r:18px;--shadow:0 12px 40px rgba(0,0,0,.35);
  --grad: linear-gradient(135deg,var(--brand) 0%,var(--brand-2) 100%);
}
:root[data-theme="light"]{
  --bg:#f4f6fc;--bg2:#eef2fa;--panel:rgba(255,255,255,.7);--panel-solid:#fff;
  --ink:#0f1730;--ink2:#3a4366;--muted:#6b7390;--line:rgba(15,23,48,.08);
  --shadow:0 10px 28px rgba(20,32,80,.08);
}
@media(prefers-color-scheme:light){:root[data-theme="auto"]{
  --bg:#f4f6fc;--bg2:#eef2fa;--panel:rgba(255,255,255,.7);--panel-solid:#fff;
  --ink:#0f1730;--ink2:#3a4366;--muted:#6b7390;--line:rgba(15,23,48,.08);
  --shadow:0 10px 28px rgba(20,32,80,.08);
}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;height:100%}
body{
  font:14.5px/1.5 -apple-system,BlinkMacSystemFont,"Inter","Segoe UI",Roboto,sans-serif;
  color:var(--ink);background:var(--bg);
  background-image:
    radial-gradient(1200px 600px at 10% -10%,color-mix(in srgb,var(--brand) 18%,transparent),transparent 60%),
    radial-gradient(1000px 500px at 110% 10%,color-mix(in srgb,var(--brand-2) 14%,transparent),transparent 55%);
  background-attachment:fixed;min-height:100vh;
}
header{
  position:sticky;top:0;z-index:10;backdrop-filter:saturate(140%) blur(16px);
  -webkit-backdrop-filter:saturate(140%) blur(16px);
  background:color-mix(in srgb,var(--bg) 80%,transparent);
  border-bottom:1px solid var(--line);padding:12px 18px;display:flex;align-items:center;gap:12px;
}
.logo{width:34px;height:34px;border-radius:10px;background:var(--grad);
  display:grid;place-items:center;color:#fff;font-weight:800;font-size:14px;
  box-shadow:0 6px 20px color-mix(in srgb,var(--brand) 40%,transparent);}
.title{font-weight:700;font-size:15px;letter-spacing:.2px}
.sub{font-size:11px;color:var(--muted)}
.spacer{flex:1}
.pill{display:inline-flex;align-items:center;gap:6px;padding:5px 10px;border-radius:99px;
  background:var(--panel);border:1px solid var(--line);font-size:12px;color:var(--ink2)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--muted)}
.dot.on{background:var(--ok);box-shadow:0 0 0 3px color-mix(in srgb,var(--ok) 25%,transparent)}
.iconbtn{width:34px;height:34px;border-radius:10px;border:1px solid var(--line);background:var(--panel);
  color:var(--ink);display:grid;place-items:center;cursor:pointer;transition:.15s}
.iconbtn:hover{border-color:var(--brand);color:var(--brand)}
.tabs{display:flex;gap:6px;padding:10px 14px;overflow-x:auto;scrollbar-width:none}
.tabs::-webkit-scrollbar{display:none}
.tab{padding:8px 14px;border:1px solid var(--line);background:var(--panel);color:var(--ink2);
  border-radius:99px;cursor:pointer;font:inherit;font-size:13px;white-space:nowrap;transition:.18s}
.tab:hover{color:var(--ink);border-color:color-mix(in srgb,var(--brand) 30%,var(--line))}
.tab.active{background:var(--grad);color:#fff;border-color:transparent;
  box-shadow:0 6px 18px color-mix(in srgb,var(--brand) 30%,transparent)}
main{padding:14px;padding-bottom:60px}
.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:12px}
.card{
  background:var(--panel);border:1px solid var(--line);border-radius:var(--r);
  padding:16px;display:flex;flex-direction:column;gap:8px;min-height:108px;
  position:relative;overflow:hidden;backdrop-filter:blur(14px);
  -webkit-backdrop-filter:blur(14px);transition:.2s;
}
.card:hover{transform:translateY(-1px);border-color:color-mix(in srgb,var(--brand) 25%,var(--line));
  box-shadow:var(--shadow)}
.card::before{content:"";position:absolute;inset:0;border-radius:inherit;pointer-events:none;
  background:linear-gradient(180deg,rgba(255,255,255,.05),transparent 40%);opacity:.6}
.card.c-success{border-color:color-mix(in srgb,var(--ok) 40%,var(--line))}
.card.c-warning{border-color:color-mix(in srgb,var(--warn) 40%,var(--line))}
.card.c-danger {border-color:color-mix(in srgb,var(--err) 40%,var(--line))}
.card.c-info   {border-color:color-mix(in srgb,var(--info) 40%,var(--line))}
.card.c-primary{border-color:color-mix(in srgb,var(--brand) 40%,var(--line))}
.lbl{font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:.6px;font-weight:600}
.val{font-size:28px;font-weight:700;line-height:1.1;font-variant-numeric:tabular-nums;
  font-family:"SF Mono",ui-monospace,Menlo,Consolas,monospace;color:var(--ink);
  background:linear-gradient(180deg,var(--ink),color-mix(in srgb,var(--ink) 70%,var(--brand)));
  -webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent}
.unit{font-size:13px;color:var(--muted);margin-left:4px;-webkit-text-fill-color:var(--muted);font-weight:500}
.btn{padding:10px 16px;border-radius:12px;border:0;background:var(--grad);color:#fff;
  font:inherit;font-weight:700;cursor:pointer;transition:.18s;min-height:42px;
  box-shadow:0 4px 14px color-mix(in srgb,var(--brand) 30%,transparent)}
.btn:hover{transform:translateY(-1px);box-shadow:0 8px 22px color-mix(in srgb,var(--brand) 40%,transparent)}
.btn:active{transform:translateY(0)}
.btn.ghost{background:var(--panel);color:var(--ink);border:1px solid var(--line);box-shadow:none}
.btn.danger{background:linear-gradient(135deg,#ff5470,#ff7e7e);box-shadow:0 4px 14px rgba(255,80,100,.3)}
.toggle{position:relative;width:54px;height:30px;background:var(--line);border-radius:99px;
  cursor:pointer;align-self:flex-start;transition:.2s;border:1px solid var(--line)}
.toggle::after{content:"";position:absolute;left:2px;top:1px;width:24px;height:24px;
  background:#fff;border-radius:50%;transition:.22s cubic-bezier(.4,1.4,.6,1);
  box-shadow:0 2px 6px rgba(0,0,0,.25)}
.toggle.on{background:var(--grad);border-color:transparent}
.toggle.on::after{left:26px}
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
  <div><div class="title">__TITLE__</div><div class="sub">JouleDash · live</div></div>
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
  if(m.type==="layout"){layout=m;applyTheme(m.theme);renderTabs();render()}
  else if(m.type==="upd"){m.cards.forEach(c=>{values[c.id]=c.value;updateCard(c.id,c.value)})}
  else if(m.type==="notify"){toast(m.level,m.message,m.ttl)}
}
function applyTheme(t){
  const stored=localStorage.getItem("joule-theme");const mode=stored||t||"auto";
  document.documentElement.setAttribute("data-theme",mode);
}
$("#themeBtn").onclick=()=>{
  const cur=document.documentElement.getAttribute("data-theme")||"auto";
  const next=cur==="auto"?"dark":(cur==="dark"?"light":"auto");
  localStorage.setItem("joule-theme",next);applyTheme(next);toast("info","Theme: "+next,1500);
};

function renderTabs(){
  const t=$("#tabs"),list=layout.tabs&&layout.tabs.length?layout.tabs:["Main"];
  if(!currentTab||!list.includes(currentTab))currentTab=list[0];
  t.innerHTML=list.map(x=>`<button class="tab ${x===currentTab?"active":""}" data-t="${x}">${x}</button>`).join("");
  t.querySelectorAll(".tab").forEach(b=>b.onclick=()=>{currentTab=b.dataset.t;renderTabs();render()});
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
