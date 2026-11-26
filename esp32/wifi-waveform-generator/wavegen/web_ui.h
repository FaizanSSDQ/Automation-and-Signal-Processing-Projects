/**
 * @file    web_ui.h
 * @brief   The generator's control page: waveform, frequency, level, sweep and
 *          a preview of the samples the DACs really output.
 *
 * Plain HTML/CSS/JS, no external files. Every change is sent to /api/set; the
 * preview is fetched from /api/preview, which runs the same DDS code as the
 * output. Open with ?demo to preview the page without the board.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef WEB_UI_H
#define WEB_UI_H

static const char WEB_UI_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Waveform Generator</title>
<style>
:root{--bg:#0f1318;--card:#161c23;--line:#28313b;--text:#e6ecf2;--muted:#8a97a5;--accent:#f0a33a;--trace:#ffb648;--sync:#5b8def;--ok:#48bb78}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 -apple-system,Segoe UI,Roboto,Arial,sans-serif}
.app{max-width:980px;margin:0 auto;padding:14px 16px 22px}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}
h1{font-size:18px;margin:0}
.pill{padding:4px 12px;border-radius:999px;font-size:13px;font-weight:600;background:#1f2731;color:var(--muted);cursor:pointer;border:0;font-family:inherit}
.pill.on{background:rgba(72,187,120,.16);color:var(--ok)}
.screen{background:#0a0d10;border:1px solid var(--line);border-radius:14px;padding:10px}
canvas{display:block;width:100%;aspect-ratio:2.6/1}
.readout{display:flex;justify-content:space-between;flex-wrap:wrap;gap:8px;margin:8px 4px 0;color:var(--muted);font-size:13px}
.readout b{color:var(--text);font-size:20px;font-variant-numeric:tabular-nums}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:12px 14px}
.card h2{font-size:13px;margin:0 0 10px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
.waves{display:grid;grid-template-columns:repeat(5,1fr);gap:6px}
.waves button,.seg button{font:inherit;font-size:13px;font-weight:600;padding:8px 0;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--muted);cursor:pointer}
.waves button.on,.seg button.on{background:var(--accent);border-color:var(--accent);color:#1d1203}
.row{display:flex;align-items:center;gap:10px;margin:10px 0}
.row span:first-child{width:92px;color:var(--muted);font-size:14px}
.row input[type=range]{flex:1;accent-color:var(--accent)}
.row output{width:84px;text-align:right;font-variant-numeric:tabular-nums}
input[type=number]{width:110px;font:inherit;padding:6px 8px;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--text)}
.presets{display:flex;gap:6px;flex-wrap:wrap}
.presets button{font:inherit;font-size:13px;padding:5px 10px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--text);cursor:pointer}
.seg{display:grid;grid-template-columns:repeat(3,1fr);gap:6px}
@media(max-width:720px){.grid{grid-template-columns:1fr}}
</style></head><body><div class="app">
<header><h1>Waveform Generator</h1><button id="out" class="pill">output</button></header>
<div class="screen"><canvas id="c" width="1300" height="500"></canvas>
<div class="readout"><span>frequency <b id="r_f">&ndash;</b></span><span>generated <b id="r_a">&ndash;</b></span>
<span>level <b id="r_l">&ndash;</b></span><span id="r_n"></span></div></div>
<div class="grid">
<div class="card"><h2>Waveform</h2>
<div class="waves" id="waves"><button data-w="sine">Sine</button><button data-w="square">Square</button>
<button data-w="triangle">Triangle</button><button data-w="saw">Saw</button><button data-w="noise">Noise</button></div>
<div class="row"><span>Frequency</span><input type="number" id="freq" min="0.01" max="20000" step="any"> Hz</div>
<div class="presets" id="pre"><button>50</button><button>100</button><button>440</button><button>1000</button><button>5000</button><button>10000</button></div>
<div class="row"><span>Amplitude</span><input type="range" id="amp" min="0" max="3300" step="50"><output id="o_amp"></output></div>
<div class="row"><span>Offset</span><input type="range" id="offset" min="0" max="3300" step="50"><output id="o_offset"></output></div>
<div class="row"><span>Duty</span><input type="range" id="duty" min="1" max="99"><output id="o_duty"></output></div>
<div class="row"><span>Anti-alias</span><input type="checkbox" id="antialias"><small style="color:var(--muted)">PolyBLEP on square and saw edges</small></div>
</div>
<div class="card"><h2>Sweep</h2>
<div class="seg" id="sweep"><button data-s="off">Off</button><button data-s="lin">Linear</button><button data-s="log">Log</button></div>
<div class="row"><span>Start</span><input type="number" id="sweep_start" min="0.01" max="20000" step="any"> Hz</div>
<div class="row"><span>Stop</span><input type="number" id="sweep_stop" min="0.01" max="20000" step="any"> Hz</div>
<div class="row"><span>Duration</span><input type="number" id="sweep_ms" min="10" max="600000" step="10"> ms</div>
<p style="color:var(--muted);font-size:13px;margin:10px 0 0">GPIO 25: waveform (8-bit DAC, 100 k samples/s) &middot; GPIO 26: sync, high for the first half of each cycle.
Add an RC low-pass (1 k&Omega;, 10 nF) on the output to smooth the steps.</p>
</div></div></div>
<script>
const demo=location.search.includes('demo'),$=id=>document.getElementById(id);
let S={wave:'sine',freq:1000,actual:1000,amp:2000,offset:1650,duty:50,output:1,antialias:1,sweep:'off',sweep_start:100,sweep_stop:10000,sweep_ms:2000,fs:100000};
const fmtHz=f=>f>=1000?(f/1000).toFixed(f%1000?3:1).replace(/\.?0+$/,'')+' kHz':(+f.toFixed(3))+' Hz';
function paint(){
 document.querySelectorAll('#waves button').forEach(b=>b.classList.toggle('on',b.dataset.w==S.wave));
 document.querySelectorAll('#sweep button').forEach(b=>b.classList.toggle('on',b.dataset.s==S.sweep));
 ['freq','sweep_start','sweep_stop','sweep_ms'].forEach(k=>{if(document.activeElement!==$(k))$(k).value=S[k]});
 ['amp','offset','duty'].forEach(k=>{$(k).value=S[k];$('o_'+k).textContent=S[k]+(k=='duty'?' %':' mV')});
 $('antialias').checked=!!S.antialias;$('out').textContent=S.output?'output on':'output off';$('out').className='pill'+(S.output?' on':'');
 $('r_f').textContent=S.sweep=='off'?fmtHz(+S.freq):fmtHz(+S.sweep_start)+' → '+fmtHz(+S.sweep_stop);
 $('r_a').textContent=S.sweep=='off'?fmtHz(+S.actual):S.sweep+' sweep';
 $('r_l').textContent=(S.amp/1000).toFixed(2)+' Vpp';}
async function set(q){if(demo){Object.assign(S,q);if(q.freq)S.actual=+q.freq;paint();preview();return}
 const r=await fetch('/api/set?'+new URLSearchParams(q));if(r.ok){S=await r.json();paint();preview()}}
$('waves').onclick=e=>{if(e.target.dataset.w)set({wave:e.target.dataset.w})};
$('sweep').onclick=e=>{if(e.target.dataset.s)set({sweep:e.target.dataset.s})};
$('pre').onclick=e=>{if(e.target.tagName=='BUTTON')set({freq:e.target.textContent})};
['freq','sweep_start','sweep_stop','sweep_ms'].forEach(k=>$(k).onchange=e=>set({[k]:e.target.value}));
['amp','offset','duty'].forEach(k=>{$(k).oninput=e=>$('o_'+k).textContent=e.target.value+(k=='duty'?' %':' mV');$(k).onchange=e=>set({[k]:e.target.value})});
$('antialias').onchange=e=>set({antialias:e.target.checked?1:0});
$('out').onclick=()=>set({output:S.output?0:1});
/* ---- preview: the DAC codes of two cycles ---- */
function demoCodes(){const f=S.sweep=='off'?+S.freq:+S.sweep_start,span=Math.min(Math.round(2*S.fs/f),S.fs*0.2),m=Math.min(span,256),out=[];
 const amp=S.amp*255/3300,off=S.offset*255/3300;
 for(let k=0;k<m;k++){const i=Math.floor(k*span/m),p=(i*f/S.fs)%1;let w;
  if(S.wave=='sine')w=Math.sin(2*Math.PI*p);else if(S.wave=='square')w=p<S.duty/100?1:-1;
  else if(S.wave=='triangle')w=p<.5?4*p-1:3-4*p;else if(S.wave=='saw')w=2*p-1;else w=Math.random()*2-1;
  out.push(S.output?Math.max(0,Math.min(255,Math.round(off+w*amp/2))):0)}return out}
function draw(codes){const cv=$('c'),g=cv.getContext('2d'),W=cv.width,H=cv.height,L=70,B=40,pw=W-L-10,ph=H-B-12;
 g.clearRect(0,0,W,H);g.font='22px sans-serif';g.fillStyle='#8a97a5';g.strokeStyle='#1f2a33';g.lineWidth=1.5;
 for(let i=0;i<=3;i++){const v=i*1.1,y=12+ph-v/3.3*ph;g.beginPath();g.moveTo(L,y);g.lineTo(W-10,y);g.stroke();g.fillText(v.toFixed(1)+' V',4,y+7)}
 const f=S.sweep=='off'?+S.freq:+S.sweep_start,ms=Math.min(2000/f,200);g.fillText('0',L,H-10);g.fillText(ms<1?(ms*1000).toFixed(0)+' µs':ms.toFixed(ms<10?2:0)+' ms',W-110,H-10);
 g.fillText('two cycles',W/2-50,H-10);if(!codes.length)return;
 const step=pw/codes.length;g.strokeStyle=getComputedStyle(document.documentElement).getPropertyValue('--trace');g.lineWidth=4;g.beginPath();
 codes.forEach((c,i)=>{const y=12+ph-c/255*ph,x=L+i*step;if(i)g.lineTo(x,y);else g.moveTo(x,y);g.lineTo(x+step,y)});g.stroke();
 $('r_n').textContent=codes.length<256?codes.length+' samples shown (all of them)':'256 of '+Math.round(Math.min(2*S.fs/f,S.fs*.2))+' samples shown'}
async function preview(){if(demo){draw(demoCodes());return}
 try{const p=await(await fetch('/api/preview',{cache:'no-store'})).json();draw(p.codes)}catch(e){}}
async function load(){if(!demo){try{S=await(await fetch('/api/settings')).json()}catch(e){}}
 if(demo&&location.search.includes('wave=')){S.wave=new URLSearchParams(location.search).get('wave');S.freq=S.actual=2500;S.duty=30;S.amp=2800}
 paint();preview()}
load();
</script></body></html>)HTML";

#endif /* WEB_UI_H */
