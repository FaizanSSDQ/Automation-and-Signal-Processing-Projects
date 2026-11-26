/**
 * @file    web_ui.h
 * @brief   The camera page: live stream, snapshot, camera settings and link status.
 *
 * Plain HTML/CSS/JS, no external files. After the browser has logged in, the
 * page reads the stream token from /token and opens the MJPEG stream on port
 * 81; settings go to /control and the status tiles poll /status every second.
 * Open with ?demo to preview it in a browser without the camera.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef WEB_UI_H
#define WEB_UI_H

static const char WEB_UI_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM</title>
<style>
:root{--bg:#101317;--card:#171c22;--line:#2a313a;--text:#e7ecf1;--muted:#8d99a6;--accent:#5aa2ff;--ok:#48bb78;--bad:#f56565}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 -apple-system,Segoe UI,Roboto,Arial,sans-serif}
.app{max-width:1100px;margin:0 auto;padding:14px 16px 22px}
header{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:12px}
h1{font-size:18px;margin:0}
.pill{padding:4px 12px;border-radius:999px;font-size:13px;font-weight:600;background:#1f2630;color:var(--muted)}
.pill.ok{background:rgba(72,187,120,.15);color:var(--ok)}.pill.bad{background:rgba(245,101,101,.15);color:var(--bad)}
.layout{display:grid;grid-template-columns:1fr 300px;gap:14px}
.view{background:#000;border:1px solid var(--line);border-radius:14px;overflow:hidden;aspect-ratio:4/3;display:flex;align-items:center;justify-content:center}
.view img,.view canvas{width:100%;height:100%;object-fit:contain;display:block}
.tiles{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:10px}
.tile{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:8px 10px}
.tile b{display:block;font-size:17px}.tile span{color:var(--muted);font-size:12px}
.panel{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:12px 14px}
.panel h2{font-size:14px;margin:4px 0 10px;color:var(--muted);font-weight:600;text-transform:uppercase;letter-spacing:.04em}
label{display:flex;align-items:center;justify-content:space-between;gap:10px;margin:9px 0;font-size:14px}
label input[type=range]{width:130px;accent-color:var(--accent)}
select{font:inherit;background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:8px;padding:4px 6px}
.out{min-width:30px;text-align:right;color:var(--muted);font-variant-numeric:tabular-nums}
.btns{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px}
button{font:inherit;font-weight:600;padding:9px 0;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--text);cursor:pointer}
button.primary{background:var(--accent);border-color:var(--accent);color:#08182c}
@media(max-width:820px){.layout{grid-template-columns:1fr}.tiles{grid-template-columns:repeat(2,1fr)}}
</style></head><body><div class="app">
<header><h1>ESP32-CAM</h1><span id="link" class="pill">connecting</span></header>
<div class="layout"><div>
<div class="view" id="view"><img id="stream" alt="live camera stream"></div>
<div class="tiles">
<div class="tile"><b id="t_fps">&ndash;</b><span>frames / s</span></div>
<div class="tile"><b id="t_kbps">&ndash;</b><span>bandwidth</span></div>
<div class="tile"><b id="t_res">&ndash;</b><span>resolution</span></div>
<div class="tile"><b id="t_rssi">&ndash;</b><span>Wi-Fi signal</span></div></div>
</div>
<div class="panel"><h2>Camera</h2>
<label>Resolution <select id="framesize"><option value="0">QQVGA 160&times;120</option><option value="1">QVGA 320&times;240</option>
<option value="2">CIF 400&times;296</option><option value="3">VGA 640&times;480</option><option value="4">SVGA 800&times;600</option>
<option value="5">XGA 1024&times;768</option><option value="6">HD 1280&times;720</option><option value="7">SXGA 1280&times;1024</option>
<option value="8">UXGA 1600&times;1200</option></select></label>
<label>JPEG quality <input type="range" id="quality" min="10" max="63"><span class="out" id="o_quality"></span></label>
<label>Frame-rate limit <input type="range" id="fps" min="0" max="30"><span class="out" id="o_fps"></span></label>
<label>Brightness <input type="range" id="brightness" min="-2" max="2"><span class="out" id="o_brightness"></span></label>
<label>Contrast <input type="range" id="contrast" min="-2" max="2"><span class="out" id="o_contrast"></span></label>
<label>Saturation <input type="range" id="saturation" min="-2" max="2"><span class="out" id="o_saturation"></span></label>
<label>Flash LED <input type="range" id="flash" min="0" max="100" step="5"><span class="out" id="o_flash"></span></label>
<label>Flip vertical <input type="checkbox" id="vflip"></label>
<label>Mirror <input type="checkbox" id="hmirror"></label>
<div class="btns"><button class="primary" id="snap">Snapshot</button><button id="pause">Pause</button></div>
<p id="info" style="color:var(--muted);font-size:13px;margin:12px 0 0"></p>
</div></div></div>
<script>
const demo=location.search.includes('demo'),$=id=>document.getElementById(id);
const ranges=['quality','fps','brightness','contrast','saturation','flash'],checks=['vflip','hmirror'];
let streamUrl='',paused=false,loaded=false;
function show(k,v){if($('o_'+k))$('o_'+k).textContent=k=='fps'&&v==0?'off':k=='flash'?v+'%':v}
function send(k,v){show(k,v);if(!demo)fetch('/control?var='+k+'&val='+v)}
ranges.forEach(k=>$(k).oninput=e=>send(k,e.target.value));
checks.forEach(k=>$(k).onchange=e=>send(k,e.target.checked?1:0));
$('framesize').onchange=e=>send('framesize',e.target.value);
function link(ok){$('link').textContent=ok?'live':'no link';$('link').className='pill '+(ok?'ok':'bad')}
function fill(s){
 if(!loaded){ranges.forEach(k=>{$(k).value=s[k];show(k,s[k])});checks.forEach(k=>$(k).checked=!!s[k]);$('framesize').value=s.framesize;loaded=true}
 $('t_fps').textContent=s.clients?s.fps_now.toFixed(1):'–';
 $('t_kbps').textContent=s.clients?(s.kbps>=1000?(s.kbps/1000).toFixed(1)+' Mbit/s':s.kbps+' kbit/s'):'–';
 $('t_res').textContent=s.resolution.split(' ')[0];
 $('t_rssi').textContent=s.rssi?s.rssi+' dBm':'access point';
 const up=s.uptime,h=Math.floor(up/3600),m=Math.floor(up%3600/60);
 $('info').textContent=s.clients+' viewer(s) · '+s.frames+' frames · up '+h+' h '+m+' min · free RAM '+Math.round(s.heap/1024)+' KB, PSRAM '+Math.round(s.psram/1024)+' KB';
}
async function poll(){
 if(demo){fill(demoStatus());link(true);return}
 try{const r=await fetch('/status',{cache:'no-store'});fill(await r.json());link(true)}catch(e){link(false)}}
async function start(){
 if(demo){startDemo();return}
 try{const t=await(await fetch('/token',{cache:'no-store'})).json();
  streamUrl=location.protocol+'//'+location.hostname+':'+t.port+'/stream?token='+t.token;$('stream').src=streamUrl}catch(e){link(false)}}
$('pause').onclick=()=>{paused=!paused;$('pause').textContent=paused?'Resume':'Pause';
 if(!demo)$('stream').src=paused?'':streamUrl};
$('snap').onclick=()=>{if(demo){const a=document.createElement('a');a.href=$('cv').toDataURL('image/jpeg');a.download='capture.jpg';a.click()}
 else window.open('/capture?t='+Date.now(),'_blank')};
/* ---- demo: a moving test pattern instead of the camera ---- */
let ds={framesize:3,quality:12,brightness:0,contrast:0,saturation:0,vflip:0,hmirror:0,flash:0,fps:15,resolution:'VGA 640x480',
 fps_now:15,kbps:2390,frames:0,clients:1,uptime:5025,heap:183000,psram:3967000,rssi:-58};
function demoStatus(){ds.frames+=15;ds.uptime++;ds.fps_now=14.6+Math.random()*.6;ds.kbps=Math.round(2300+Math.random()*180);return ds}
function startDemo(){
 const c=document.createElement('canvas');c.id='cv';c.width=640;c.height=480;$('view').replaceChildren(c);const g=c.getContext('2d');let f=0;
 const bars=['#c0c0c0','#c0c000','#00c0c0','#00c000','#c000c0','#c00000','#0000c0'];
 (function draw(){if(!paused){f++;bars.forEach((b,i)=>{g.fillStyle=b;g.fillRect(i*640/7,0,640/7+1,330)});
  const grd=g.createLinearGradient(0,0,640,0);grd.addColorStop(0,'#000');grd.addColorStop(1,'#fff');g.fillStyle=grd;g.fillRect(0,330,640,70);
  g.fillStyle='#111';g.fillRect(0,400,640,80);g.fillStyle='#5aa2ff';g.beginPath();g.arc(60+(f*4)%520,440,24,0,7);g.fill();
  g.fillStyle='#fff';g.font='bold 22px monospace';g.fillText('ESP32-CAM demo  frame '+f,150,448)}
  setTimeout(draw,66)})()}
start();poll();setInterval(poll,1000);
</script></body></html>)HTML";

#endif /* WEB_UI_H */
