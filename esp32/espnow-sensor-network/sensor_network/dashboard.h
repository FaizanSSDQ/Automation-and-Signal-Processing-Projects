/**
 * @file    dashboard.h
 * @brief   The gateway's live dashboard (one card per sensor node).
 *
 * Polls /api/nodes every 2 s. Self-contained HTML/CSS/JS so it works on the
 * gateway's own access point without internet. Open with ?demo to preview it
 * in a browser without hardware.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef DASHBOARD_H
#define DASHBOARD_H

static const char DASHBOARD_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sensor Network</title>
<style>
:root{--bg:#f4f6f8;--card:#fff;--line:#e2e8f0;--text:#1a202c;--muted:#718096;--accent:#2b6cb0;--ok:#38a169;--bad:#e53e3e;--warn:#dd6b20}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 -apple-system,Segoe UI,Roboto,Arial,sans-serif}
header{background:var(--card);border-bottom:1px solid var(--line);padding:14px 20px;display:flex;justify-content:space-between;align-items:center}
h1{margin:0;font-size:19px}.sub{color:var(--muted);font-size:13px}
main{max-width:1100px;margin:0 auto;padding:18px 16px;display:grid;gap:14px;grid-template-columns:repeat(auto-fill,minmax(300px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:16px 18px;box-shadow:0 2px 10px rgba(0,0,0,.04)}
.card.off{opacity:.55}
.top{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}
.top b{font-size:16px}.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;background:var(--ok)}
.off .dot{background:var(--bad)}.age{color:var(--muted);font-size:12.5px}
.big{display:flex;gap:22px;margin-bottom:10px}.big div{font-size:28px;font-weight:700}.big small{display:block;font-size:12px;color:var(--muted);font-weight:500}
.rows{display:grid;grid-template-columns:1fr 1fr;gap:6px 14px;font-size:14px}.rows span{color:var(--muted)}
.bar{height:6px;background:#edf2f7;border-radius:3px;margin-top:4px;overflow:hidden}.bar i{display:block;height:100%;background:var(--accent)}
.chips{display:flex;gap:6px;margin:12px 0 8px;flex-wrap:wrap}.chip{padding:2px 10px;border-radius:999px;font-size:12.5px;background:#edf2f7;color:var(--muted)}
.chip.on{background:#fed7d7;color:#c53030;font-weight:600}
.foot{border-top:1px solid var(--line);padding-top:8px;display:flex;justify-content:space-between;color:var(--muted);font-size:12.5px}
.empty{grid-column:1/-1;text-align:center;color:var(--muted);padding:60px 0}
</style></head><body>
<header><div><h1>Sensor Network</h1><div class="sub" id="sum">waiting for nodes</div></div><div class="sub" id="upd"></div></header>
<main id="grid"><div class="empty">No node has reported yet.</div></main>
<script>
const demo=location.search.includes("demo"),WAKE=["timer","alarm","power-on"];
function card(n){const al=[["flame",1],["vibration",2],["motion",4]].map(([k,b])=>`<span class="chip ${n.alarms&b?"on":""}">${k}</span>`).join("");
 const bat=n.bat/1000,bp=Math.max(0,Math.min(100,(bat-3.3)/0.9*100));
 return `<div class="card ${n.online?"":"off"}"><div class="top"><b><span class="dot"></span>Node ${n.id}</b><span class="age">${n.online?n.age+" s ago":"offline"}</span></div>
 <div class="big"><div>${n.dht?n.t.toFixed(1)+" °C":"--"}<small>temperature</small></div><div>${n.dht?n.h.toFixed(0)+" %":"--"}<small>humidity</small></div></div>
 <div class="rows"><div><span>Pressure</span><br>${n.bmp?(n.p/100).toFixed(1)+" hPa":"--"}</div><div><span>BMP280 temp.</span><br>${n.bmp?n.bt.toFixed(2)+" °C":"--"}</div>
 <div><span>Battery</span><br>${bat.toFixed(2)} V<div class="bar"><i style="width:${bp}%"></i></div></div><div><span>Signal</span><br>${n.rssi} dBm</div></div>
 <div class="chips">${al}</div>
 <div class="foot"><span>${n.rx} reports · loss ${n.loss}%</span><span>#${n.seq} · ${WAKE[n.wake]||""}</span></div></div>`;}
function render(d){const g=document.getElementById("grid");
 g.innerHTML=d.nodes.length?d.nodes.map(card).join(""):'<div class="empty">No node has reported yet.</div>';
 const on=d.nodes.filter(n=>n.online).length;document.getElementById("sum").textContent=`${on} of ${d.nodes.length} nodes online · ${d.bad_frames} bad frames`;
 document.getElementById("upd").textContent="updated "+new Date().toLocaleTimeString();}
function fake(){const s=Date.now()/1000;return{bad_frames:0,nodes:[
 {id:1,dht:1,bmp:1,t:24.6+Math.sin(s/30),h:48.2,p:100912,bt:24.81,bat:4020,alarms:0,wake:0,seq:1287,rssi:-54,rx:1286,lost:1,loss:0.1,age:12,online:true},
 {id:2,dht:1,bmp:1,t:31.2,h:35.0,p:100874,bt:31.40,bat:3710,alarms:1,wake:1,seq:944,rssi:-71,rx:930,lost:14,loss:1.5,age:2,online:true},
 {id:3,dht:0,bmp:1,t:0,h:0,p:100903,bt:22.15,bat:3390,alarms:4,wake:0,seq:402,rssi:-83,rx:388,lost:14,loss:3.5,age:251,online:false}]};}
async function tick(){if(demo){render(fake());return;}try{const r=await fetch("/api/nodes",{cache:"no-store"});render(await r.json());}catch(e){}}
tick();setInterval(tick,2000);
</script></body></html>)HTML";

#endif /* DASHBOARD_H */
