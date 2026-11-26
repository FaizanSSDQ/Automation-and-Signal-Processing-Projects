/**
 * @file    web_ui.h
 * @brief   The control page served by the car (joystick, modes, telemetry).
 *
 * Plain HTML/CSS/JS, no external files, so it works on the car's own Wi-Fi
 * network without internet. The page sends /cmd every 100 ms while open; the
 * firmware stops the car if these messages stop (link watchdog).
 * Open with ?demo to preview it in a browser without the car.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef WEB_UI_H
#define WEB_UI_H

static const char WEB_UI_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Robot Car</title>
<style>
:root{--bg:#0f1419;--card:#171d24;--line:#2a323c;--text:#e6edf3;--muted:#8b98a5;--accent:#63a4ff;--ok:#38a169;--warn:#dd6b20;--bad:#e53e3e}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 -apple-system,Segoe UI,Roboto,Arial,sans-serif;user-select:none}
.app{max-width:440px;margin:0 auto;padding:14px 16px 20px}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}
h1{font-size:18px;margin:0}
.pill{padding:4px 12px;border-radius:999px;font-size:13px;font-weight:600;background:#1f2a36;color:var(--muted)}
.pill.ok{background:rgba(56,161,105,.18);color:#68d391}.pill.bad{background:rgba(229,62,62,.18);color:#fc8181}
.modes{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin-bottom:12px}
.modes button{font:inherit;font-weight:600;padding:10px 0;border-radius:12px;border:1px solid var(--line);background:var(--card);color:var(--muted)}
.modes button.on{background:var(--accent);border-color:var(--accent);color:#0b1a2e}
.pad{position:relative;width:min(78vw,300px);aspect-ratio:1;margin:6px auto 12px;border-radius:50%;
 background:radial-gradient(circle,#1d2631 0%,#141a21 70%);border:2px solid var(--line);touch-action:none}
.pad::before,.pad::after{content:"";position:absolute;background:var(--line)}
.pad::before{left:50%;top:12%;bottom:12%;width:1px}.pad::after{top:50%;left:12%;right:12%;height:1px}
.knob{position:absolute;width:30%;aspect-ratio:1;left:35%;top:35%;border-radius:50%;background:var(--accent);
 box-shadow:0 6px 18px rgba(99,164,255,.35);z-index:1}
.row{display:flex;align-items:center;gap:10px;margin:4px 0 12px;color:var(--muted)}
.row input{flex:1;accent-color:var(--accent)}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.tile{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:10px 12px}
.tile small{display:block;color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.05em}
.tile b{font-size:20px}
.bar{height:6px;border-radius:3px;background:#222c37;margin-top:6px;overflow:hidden}
.bar i{display:block;height:100%;background:var(--accent);width:0}
.wheel{display:flex;gap:6px;align-items:center;margin-top:4px}.wheel .bar{flex:1;margin:0;position:relative}
.wheel .bar i{position:absolute;left:50%}
.dots{display:flex;gap:10px;margin-top:6px}.dot{width:16px;height:16px;border-radius:50%;background:#222c37;border:1px solid var(--line)}
.dot.on{background:var(--warn);border-color:var(--warn)}
.stop{margin-top:10px;text-align:center;color:#fc8181;font-weight:600;min-height:20px}
</style></head><body><div class="app">
<header><h1>Robot Car</h1><span id="link" class="pill">connecting</span></header>
<div class="modes"><button data-m="manual" class="on">Manual</button><button data-m="avoid">Avoid</button><button data-m="line">Line</button></div>
<div class="pad" id="pad"><div class="knob" id="knob"></div></div>
<div class="row">Speed<input id="max" type="range" min="10" max="100" value="70"><span id="maxv">70 %</span></div>
<div class="grid">
 <div class="tile"><small>Distance</small><b id="dist">--</b><div class="bar"><i id="distb"></i></div></div>
 <div class="tile"><small>Battery</small><b id="bat">--</b><div class="bar"><i id="batb"></i></div></div>
 <div class="tile"><small>Wheels L / R</small>
  <div class="wheel">L<div class="bar"><i id="wl"></i></div></div><div class="wheel">R<div class="bar"><i id="wr"></i></div></div></div>
 <div class="tile"><small>Line sensors</small><div class="dots"><span class="dot" id="irl"></span><span class="dot" id="irr"></span></div>
  <small id="rssi" style="margin-top:6px">signal --</small></div>
</div>
<div class="stop" id="stop"></div>
</div>
<script>
const demo=location.search.includes("demo");
let x=0,y=0,mode="manual",fails=0,busy=false,t0=Date.now();
const pad=document.getElementById("pad"),knob=document.getElementById("knob"),$=id=>document.getElementById(id);
function setKnob(){knob.style.left=(35+x*0.35)+"%";knob.style.top=(35-y*0.35)+"%";}
function move(e){const r=pad.getBoundingClientRect();let dx=(e.clientX-r.left)/r.width*2-1,dy=1-(e.clientY-r.top)/r.height*2;
 const m=Math.hypot(dx,dy);if(m>1){dx/=m;dy/=m;}x=Math.round(dx*100);y=Math.round(dy*100);setKnob();}
pad.addEventListener("pointerdown",e=>{pad.setPointerCapture(e.pointerId);move(e);});
pad.addEventListener("pointermove",e=>{if(e.buttons)move(e);});
["pointerup","pointercancel"].forEach(ev=>pad.addEventListener(ev,()=>{x=0;y=0;setKnob();}));
document.querySelectorAll(".modes button").forEach(b=>b.onclick=()=>{mode=b.dataset.m;
 document.querySelectorAll(".modes button").forEach(o=>o.classList.toggle("on",o===b));});
$("max").oninput=()=>$("maxv").textContent=$("max").value+" %";
function wheel(el,v){el.style.width=Math.abs(v)/2+"%";el.style.left=v>=0?"50%":(50-Math.abs(v)/2)+"%";}
function show(t){$("dist").textContent=t.dok?t.d.toFixed(0)+" cm":"clear";$("distb").style.width=(t.dok?Math.min(t.d,150)/1.5:100)+"%";
 $("bat").textContent=t.bat.toFixed(2)+" V";$("batb").style.width=Math.max(0,Math.min(100,(t.bat-6.0)/2.4*100))+"%";
 wheel($("wl"),t.l);wheel($("wr"),t.r);$("irl").classList.toggle("on",!!t.irl);$("irr").classList.toggle("on",!!t.irr);
 $("rssi").textContent="signal "+(t.rssi?t.rssi+" dBm":"--");$("stop").textContent=t.stop?"Stopped: "+t.stop:"";}
function link(ok){const p=$("link");p.textContent=ok?"connected":"no link";p.className="pill "+(ok?"ok":"bad");}
async function tick(){const q="/cmd?x="+x+"&y="+y+"&max="+$("max").value+"&mode="+mode;
 if(demo){const s=(Date.now()-t0)/1000;show({d:60+40*Math.sin(s/2),dok:1,bat:7.62,l:Math.round(y*0.7+x*0.3),r:Math.round(y*0.7-x*0.3),
  irl:mode==="line"&&Math.sin(s)>0.6?1:0,irr:0,rssi:-52,stop:""});link(true);return;}
 if(busy)return;busy=true;
 try{const r=await fetch(q,{cache:"no-store"});show(await r.json());fails=0;link(true);}catch(e){if(++fails>2)link(false);}
 busy=false;}
setInterval(tick,100);setKnob();
</script></body></html>)HTML";

#endif /* WEB_UI_H */
