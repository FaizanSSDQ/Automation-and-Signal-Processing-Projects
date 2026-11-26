/**
 * @file    web_ui.h
 * @brief   The radar display served by the ESP32 (replaces the Processing sketch).
 *
 * Plain HTML/CSS/JS with a canvas, no external files, so it works on the
 * radar's own Wi-Fi network. It polls /api/scan four times a second and draws
 * the sweep line, the ultrasonic and infrared traces and the detected objects.
 * Open with ?demo to preview it in a browser without the hardware.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef WEB_UI_H
#define WEB_UI_H

static const char WEB_UI_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Radar</title>
<style>
:root{--bg:#0e1316;--card:#151c20;--line:#26323a;--text:#e3ece9;--muted:#86979a;--grid:#1f4d3c;--us:#3ddc97;--ir:#f0b429;--obj:#ff6b6b}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 -apple-system,Segoe UI,Roboto,Arial,sans-serif}
.app{max-width:760px;margin:0 auto;padding:14px 16px 22px}
header{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:10px}
h1{font-size:18px;margin:0}
.pill{padding:4px 12px;border-radius:999px;font-size:13px;font-weight:600;background:#1c262c;color:var(--muted)}
.pill.ok{background:rgba(61,220,151,.15);color:var(--us)}.pill.bad{background:rgba(255,107,107,.15);color:var(--obj)}
canvas{display:block;width:100%;aspect-ratio:2/1.08;background:var(--card);border:1px solid var(--line);border-radius:16px}
.legend{display:flex;gap:16px;flex-wrap:wrap;margin:10px 2px;color:var(--muted);font-size:13px}
.legend i{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;vertical-align:-1px}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin:8px 0}
.tile{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:8px 10px}
.tile b{display:block;font-size:18px}.tile span{color:var(--muted);font-size:12px}
.controls{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin:10px 0}
.controls label{color:var(--muted);font-size:14px}
input[type=number]{width:72px;font:inherit;padding:6px 8px;border-radius:10px;border:1px solid var(--line);background:var(--card);color:var(--text)}
button{font:inherit;font-weight:600;padding:7px 14px;border-radius:10px;border:1px solid var(--line);background:var(--card);color:var(--text);cursor:pointer}
table{width:100%;border-collapse:collapse;font-size:14px}
th,td{text-align:left;padding:6px 8px;border-bottom:1px solid var(--line)}th{color:var(--muted);font-weight:600}
@media(max-width:520px){.grid{grid-template-columns:repeat(2,1fr)}}
</style></head><body><div class="app">
<header><h1>180&deg; Radar</h1><span id="link" class="pill">connecting</span></header>
<canvas id="c" width="1400" height="756"></canvas>
<div class="legend"><span><i style="background:var(--us)"></i>Ultrasonic (HC-SR04)</span>
<span><i style="background:var(--ir)"></i>Infrared (Sharp, 10&ndash;80 cm)</span>
<span><i style="background:var(--obj)"></i>Detected object</span></div>
<div class="grid">
<div class="tile"><b id="ang">&ndash;</b><span>servo angle</span></div>
<div class="tile"><b id="near">&ndash;</b><span>nearest object</span></div>
<div class="tile"><b id="cnt">&ndash;</b><span>objects</span></div>
<div class="tile"><b id="swp">&ndash;</b><span>sweeps</span></div></div>
<div class="controls"><label>Air temperature <input id="temp" type="number" step="0.5" min="-20" max="50"> &deg;C</label>
<button id="setT">Set</button><button id="run">Pause</button></div>
<table><thead><tr><th>#</th><th>Bearing</th><th>Extent</th><th>Range</th></tr></thead><tbody id="tb"></tbody></table>
</div>
<script>
const MAX=200,demo=location.search.includes('demo');
const cv=document.getElementById('c'),g=cv.getContext('2d');
const W=cv.width,H=cv.height,cx=W/2,cy=H-40,R=Math.min(cx-80,cy-60);
const css=n=>getComputedStyle(document.documentElement).getPropertyValue(n).trim();
let running=true,tempSet=false,lastA=null,dirUI=1;
function pt(deg,cm){const a=deg*Math.PI/180,r=cm/MAX*R;return[cx+r*Math.cos(a),cy-r*Math.sin(a)]}
function draw(d){
 g.clearRect(0,0,W,H);g.lineWidth=2;g.strokeStyle=css('--grid');g.fillStyle=css('--muted');g.font='24px sans-serif';
 for(const cm of[50,100,150,200]){g.beginPath();g.arc(cx,cy,cm/MAX*R,Math.PI,0);g.stroke();g.fillText(cm+' cm',cx+cm/MAX*R-58,cy+30)}
 for(let a=0;a<=180;a+=30){const[x,y]=pt(a,MAX);g.beginPath();g.moveTo(cx,cy);g.lineTo(x,y);g.stroke();
  const[lx,ly]=pt(a,MAX*1.06);g.fillText(a+'°',lx-(a>90?40:a==90?18:0),ly+8)}
 /* sweep line with a fading trail behind it */
 if(lastA!==null&&d.angle!==lastA)dirUI=d.angle>lastA?1:-1;lastA=d.angle;
 for(let k=0;k<12;k++){const a=d.angle-k*dirUI*2;if(a<0||a>180)continue;
  const[x,y]=pt(a,MAX);g.strokeStyle='rgba(61,220,151,'+(0.5-k*0.04)+')';g.lineWidth=k?3:4;g.beginPath();g.moveTo(cx,cy);g.lineTo(x,y);g.stroke()}
 const trace=(arr,col,r)=>{g.fillStyle=col;arr.forEach((cm,i)=>{if(cm<0)return;const[x,y]=pt(i*d.step,cm);g.beginPath();g.arc(x,y,r,0,7);g.fill()})};
 trace(d.us,css('--us'),6);trace(d.ir,css('--ir'),5);
 g.strokeStyle=css('--obj');g.lineWidth=5;g.fillStyle=css('--obj');g.font='bold 26px sans-serif';
 d.objects.forEach((o,k)=>{g.beginPath();g.arc(cx,cy,o.range/MAX*R,-(o.to+1)*Math.PI/180,-(o.from-1)*Math.PI/180);g.stroke();
  const[x,y]=pt(o.bearing,o.range+14);g.fillText(k+1,x-8,y+8)});
}
function show(d){
 draw(d);
 const nearest=d.objects.reduce((m,o)=>Math.min(m,o.range),1e9);
 ang.textContent=d.angle+'°';cnt.textContent=d.objects.length;swp.textContent=d.sweeps;
 document.getElementById('near').textContent=d.objects.length?nearest+' cm':'–';
 if(!tempSet){temp.value=d.temp;tempSet=true}
 tb.innerHTML=d.objects.map((o,k)=>'<tr><td>'+(k+1)+'</td><td>'+o.bearing+'°</td><td>'+o.from+'–'+o.to+'°</td><td>'+o.range+' cm</td></tr>').join('')||
  '<tr><td colspan="4" style="color:var(--muted)">nothing within 150 cm</td></tr>';
}
function setLink(ok){link.textContent=ok?'live':'no link';link.className='pill '+(ok?'ok':'bad')}
/* ---- demo: a box, a post and a wall, with a little noise ---- */
let dAng=0,dDir=1,dSw=0;const dUs=Array(91).fill(-1),dIr=Array(91).fill(-1);
function scene(a){if(a>=20&&a<=36)return 62+Math.abs(a-28)*.6;if(a>=78&&a<=84)return 35;if(a>=128&&a<=150)return 104;return 260}
function find(us){const out=[];let i=0;while(i<us.length){if(us[i]<0||us[i]>150){i++;continue}let j=i,n=us[i];
 while(j+1<us.length&&us[j+1]>=0&&us[j+1]<=150&&Math.abs(us[j+1]-us[j])<15){j++;n=Math.min(n,us[j])}
 out.push({from:i*2,to:j*2,bearing:i+j,range:n});i=j+1}return out}
function demoStep(){
 if(running){for(let s=0;s<3;s++){dAng+=2*dDir;if(dAng>180||dAng<0){dDir=-dDir;dAng+=4*dDir}if(dAng==0||dAng==180)dSw++;
  const t=scene(dAng),i=dAng/2;dUs[i]=t<=MAX?Math.round(t+(Math.random()-.5)*2):-1;
  dIr[i]=t>=10&&t<=80?Math.round(t+(Math.random()-.5)*4):-1}}
 return{angle:dAng,step:2,temp:25,sweeps:dSw,us:dUs,ir:dIr,objects:find(dUs)}}
if(demo)for(let k=0;k<70;k++)demoStep();   /* one sweep up and back, then part of the next */
async function poll(){
 if(demo){show(demoStep());setLink(true);return}
 try{const r=await fetch('/api/scan',{cache:'no-store'});show(await r.json());setLink(true)}catch(e){setLink(false)}}
setT.onclick=()=>{if(!demo)fetch('/api/config?temp='+encodeURIComponent(temp.value))};
run.onclick=()=>{running=!running;run.textContent=running?'Pause':'Resume';if(!demo)fetch('/api/config?run='+(running?1:0))};
poll();setInterval(poll,250);
</script></body></html>)HTML";

#endif /* WEB_UI_H */
