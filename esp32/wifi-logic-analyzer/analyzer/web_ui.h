/**
 * @file    web_ui.h
 * @brief   The analyzer's browser interface: capture settings, per-channel
 *          triggers, a zoomable 8-channel waveform view with decoded UART and
 *          I2C, measurements and VCD export.
 *
 * Plain HTML/CSS/JS, no external files. Open with ?demo to try the viewer
 * without the board (synthetic UART, PWM and I2C signals).
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef WEB_UI_H
#define WEB_UI_H

static const char WEB_UI_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Logic Analyzer</title>
<style>
:root{--bg:#0e1215;--card:#151b20;--line:#26303a;--text:#e4ebf0;--muted:#87949f;--accent:#4fd1c5;--trig:#f6ad55;--ann:#2c5282;--annt:#bee3f8;--err:#fc8181}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:14px/1.4 -apple-system,Segoe UI,Roboto,Arial,sans-serif}
.app{max-width:1200px;margin:0 auto;padding:12px 16px 20px}
header{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:10px;flex-wrap:wrap}
h1{font-size:18px;margin:0}
.bar{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
select,input{font:inherit;background:var(--card);color:var(--text);border:1px solid var(--line);border-radius:8px;padding:5px 6px}
button{font:inherit;font-weight:600;padding:6px 12px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--text);cursor:pointer}
button.primary{background:var(--accent);border-color:var(--accent);color:#06221f}
label{color:var(--muted)}
.status{color:var(--muted);font-size:13px;margin:6px 2px}
.view{display:grid;grid-template-columns:150px 1fr;background:var(--card);border:1px solid var(--line);border-radius:12px;overflow:hidden}
.names{border-right:1px solid var(--line)}
.names div{height:46px;display:flex;align-items:center;justify-content:space-between;padding:0 8px;border-bottom:1px solid #1c242b}
.names div:first-child{height:28px}
.names b{font-size:13px}.names small{color:var(--muted)}
.names select{padding:1px 2px;font-size:12px}
canvas{width:100%;height:396px;display:block;cursor:grab;touch-action:none}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:10px 12px}
.card h2{font-size:12px;margin:0 0 8px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
table{width:100%;border-collapse:collapse;font-size:13px;font-variant-numeric:tabular-nums}
td,th{padding:4px 6px;text-align:right;border-bottom:1px solid var(--line)}th{color:var(--muted);font-weight:600}td:first-child,th:first-child{text-align:left}
.text{font-family:ui-monospace,Consolas,monospace;font-size:13px;background:#0b0f12;border-radius:8px;padding:8px;min-height:40px;white-space:pre-wrap;word-break:break-all}
@media(max-width:760px){.grid{grid-template-columns:1fr}.view{grid-template-columns:110px 1fr}}
</style></head><body><div class="app">
<header><h1>Logic Analyzer</h1>
<div class="bar"><label>Rate <select id="rate"></select></label>
<label>Samples <select id="samples"><option>4096</option><option selected>16384</option><option>32768</option><option>65536</option></select></label>
<label>Pre-trigger <select id="pre"><option value="1">1 %</option><option value="10" selected>10 %</option><option value="25">25 %</option><option value="50">50 %</option></select></label>
<button class="primary" id="go">Capture</button><button id="abort">Force</button></div></header>
<div class="bar"><button id="zin">Zoom +</button><button id="zout">Zoom &minus;</button><button id="fit">Fit</button><button id="trg">Go to trigger</button>
<button id="vcd">Export VCD</button><span class="status" id="st">No capture yet</span></div>
<div class="view" style="margin-top:8px"><div class="names" id="names"></div><canvas id="cv"></canvas></div>
<div class="grid">
<div class="card"><h2>Decoders</h2>
<div class="bar"><label>UART on <select id="u_ch"></select></label><label>baud <select id="u_baud"><option>9600</option><option>57600</option><option selected>115200</option><option>230400</option><option>921600</option></select></label></div>
<div class="bar" style="margin-top:6px"><label>I2C SDA <select id="i_sda"></select></label><label>SCL <select id="i_scl"></select></label><button id="dec">Decode</button></div>
<h2 style="margin-top:10px">UART text</h2><div class="text" id="utext"></div>
<h2 style="margin-top:10px">I2C transactions</h2><div class="text" id="itext"></div></div>
<div class="card"><h2>Measurements</h2><table><thead><tr><th>Channel</th><th>Frequency</th><th>Duty</th><th>Edges</th><th>Shortest pulse</th></tr></thead><tbody id="meas"></tbody></table></div>
</div></div>
<script>
const demo=location.search.includes('demo'),$=id=>document.getElementById(id);
const PINS=[13,14,16,17,18,19,21,22],RATES=[5e3,1e4,2e4,5e4,1e5,2e5,5e5,1e6,2e6];
const NAMES=['UART TX','PWM','I2C SDA','I2C SCL','CH4','CH5','CH6','CH7'];
let data=new Uint8Array(0),info=null,ann=[],x0=0,x1=1;
const fmtF=f=>f>=1e6?(f/1e6).toFixed(3)+' MHz':f>=1e3?(f/1e3).toFixed(2)+' kHz':f.toFixed(1)+' Hz';
const fmtT=s=>s>=1?s.toFixed(3)+' s':s>=1e-3?(s*1e3).toFixed(3)+' ms':(s*1e6).toFixed(2)+' µs';
RATES.forEach(r=>$('rate').add(new Option(fmtF(r).replace('.000','').replace('.00',''),r)));$('rate').value=1e6;
let html='<div><small>channel / GPIO</small><small>trigger</small></div>';
for(let c=0;c<8;c++){html+='<div><span><b>'+c+'</b> <small>GPIO '+PINS[c]+'</small></span><select id="t'+c+'"><option>x</option><option>0</option><option>1</option><option value="r">↑</option><option value="f">↓</option></select></div>';
 ['u_ch','i_sda','i_scl'].forEach(s=>$(s).add(new Option(c+' (GPIO '+PINS[c]+')',c)))}
$('names').innerHTML=html;$('u_ch').value=0;$('i_sda').value=2;$('i_scl').value=3;$('t0').value='f';
const cv=$('cv'),g=cv.getContext('2d');
function size(){const r=cv.getBoundingClientRect();cv.width=Math.round(r.width*devicePixelRatio);cv.height=Math.round(r.height*devicePixelRatio)}
function draw(){size();const W=cv.width,H=cv.height,dp=devicePixelRatio,top=28*dp,row=46*dp;g.clearRect(0,0,W,H);
 if(!data.length){g.fillStyle='#87949f';g.font=14*dp+'px sans-serif';g.fillText('Press Capture (or open with ?demo)',20*dp,top+30*dp);return}
 const n=data.length,span=x1-x0,sx=W/span,rate=info.actual_rate;
 /* time ruler */
 g.fillStyle='#87949f';g.font=11*dp+'px sans-serif';g.strokeStyle='#1c242b';g.lineWidth=1;
 const sec=span/rate,raw=sec/8,p10=Math.pow(10,Math.floor(Math.log10(raw))),step=[1,2,5,10].map(k=>k*p10).find(v=>v>=raw)*rate;
 for(let s=Math.ceil(x0/step)*step;s<x1;s+=step){const x=(s-x0)*sx;g.beginPath();g.moveTo(x,top);g.lineTo(x,H);g.stroke();g.fillText(fmtT((s-info.trigger_index)/rate),x+3*dp,18*dp)}
 /* trigger */
 const tx=(info.trigger_index-x0)*sx;g.strokeStyle=getComputedStyle(document.body).getPropertyValue('--trig');g.lineWidth=2*dp;g.beginPath();g.moveTo(tx,top-6*dp);g.lineTo(tx,H);g.stroke();
 /* decoded annotations, under the traces */
 g.font='bold '+11*dp+'px ui-monospace,monospace';
 ann.forEach(a=>{const xa=(a.s-x0)*sx,xb=(a.e-x0)*sx;if(xb<0||xa>W)return;const y=top+row*a.ch+12*dp,h=row-24*dp;
  g.fillStyle=a.err?'rgba(252,129,129,.35)':'rgba(44,82,130,.55)';g.fillRect(xa,y,Math.max(2,xb-xa),h);
  if(xb-xa>g.measureText(a.t).width+6){g.fillStyle=a.err?'#fed7d7':'#bee3f8';g.fillText(a.t,xa+3*dp,y+h/2+4*dp)}})
 /* traces: per pixel column, a level line or a full-height bar where the signal toggles */
 g.strokeStyle=getComputedStyle(document.body).getPropertyValue('--accent');g.lineWidth=1.5*dp;
 for(let c=0;c<8;c++){const yb=top+row*c+row-10*dp,yt=top+row*c+10*dp;g.beginPath();let last=-1;
  for(let px=0;px<W;px++){const a=Math.max(0,Math.floor(x0+px/sx)),b=Math.min(n,Math.max(a+1,Math.floor(x0+(px+1)/sx)));if(a>=n)break;
   let or=0,and=1;for(let i=a;i<b;i++){const v=(data[i]>>c)&1;or|=v;and&=v}
   if(or!==and){g.moveTo(px,yt);g.lineTo(px,yb);last=-1;continue}
   const y=or?yt:yb;if(last<0)g.moveTo(px,y);else if(last!==y){g.lineTo(px,last);g.lineTo(px,y)}g.lineTo(px+1,y);last=y}
  g.stroke()}
}
function clampView(){const n=data.length,span=Math.min(Math.max(x1-x0,20),n);if(x0<0)x0=0;x1=x0+span;if(x1>n){x1=n;x0=n-span}}
function zoom(f,at){const c=at===undefined?(x0+x1)/2:at,span=(x1-x0)*f;x0=c-(c-x0)*f;x1=x0+span;clampView();draw()}
$('zin').onclick=()=>zoom(0.5);$('zout').onclick=()=>zoom(2);$('fit').onclick=()=>{x0=0;x1=data.length;draw()};
$('trg').onclick=()=>{if(!info)return;const span=Math.min(400,data.length);x0=info.trigger_index-span*0.2;x1=x0+span;clampView();draw()};
cv.onwheel=e=>{e.preventDefault();const r=cv.getBoundingClientRect();zoom(e.deltaY<0?0.7:1.4,x0+(e.clientX-r.left)/r.width*(x1-x0))};
let drag=null;cv.onpointerdown=e=>{drag={x:e.clientX,a:x0,b:x1};cv.setPointerCapture(e.pointerId)};
cv.onpointermove=e=>{if(!drag)return;const d=(e.clientX-drag.x)/cv.getBoundingClientRect().width*(drag.b-drag.a);x0=drag.a-d;x1=drag.b-d;clampView();draw()};
cv.onpointerup=()=>drag=null;window.onresize=draw;
function showResult(){$('st').textContent=data.length+' samples at '+fmtF(info.actual_rate)+' ('+fmtT(data.length/info.actual_rate)+'), '+
 (info.triggered?'triggered':'no trigger: captured on timeout')+(info.irq_off?', recorded with interrupts off':'')+', worst sample delay '+info.max_late_ns+' ns';
 x0=0;x1=data.length;draw();measure();decode()}
function measureJS(){const out=[];for(let c=0;c<8;c++){let edges=0,high=0,rises=[],last=-1,minp=0;
 for(let i=0;i<data.length;i++){const v=(data[i]>>c)&1;high+=v;if(i&&v!==((data[i-1]>>c)&1)){edges++;if(last>=0){const r=i-last;if(!minp||r<minp)minp=r}last=i;if(v)rises.push(i)}}
 out.push({edges,duty:100*high/data.length,freq:rises.length>1?(rises.length-1)*info.actual_rate/(rises[rises.length-1]-rises[0]):0,min_pulse:minp})}return out}
async function measure(){const m=demo?measureJS():await(await fetch('/api/measure')).json();
 $('meas').innerHTML=m.map((r,c)=>'<tr><td>'+c+' · GPIO '+PINS[c]+'</td><td>'+(r.freq?fmtF(r.freq):'–')+'</td><td>'+r.duty.toFixed(1)+' %</td><td>'+r.edges+'</td><td>'+
  (r.min_pulse?fmtT(r.min_pulse/info.actual_rate):'–')+'</td></tr>').join('')}
async function decode(){ann=[];const uc=+$('u_ch').value,sda=+$('i_sda').value;
 const u=demo?demoAnn.uart:await(await fetch('/api/decode?type=uart&ch='+uc+'&baud='+$('u_baud').value)).json();
 const i2=demo?demoAnn.i2c:await(await fetch('/api/decode?type=i2c&sda='+sda+'&scl='+$('i_scl').value)).json();
 const chr=v=>v>=32&&v<127?String.fromCharCode(v):v==10?'\\n':'0x'+v.toString(16).padStart(2,'0');
 u.forEach(f=>ann.push({ch:uc,s:f.s,e:f.e,t:chr(f.v),err:f.err}));
 $('utext').textContent=u.map(f=>f.v==10?'\n':f.v>=32&&f.v<127?String.fromCharCode(f.v):'['+f.v.toString(16)+']').join('')||'(nothing decoded)';
 const hx=v=>'0x'+v.toString(16).toUpperCase().padStart(2,'0');let lines=[],cur='';
 i2.forEach(e=>{const t=e.k=='addr'?hx(e.v)+(e.rd?' R':' W')+(e.ack?'':' NACK'):e.k=='data'?hx(e.v)+(e.ack?'':' NACK'):e.k=='start'?'S':e.k=='restart'?'Sr':'P';
  ann.push({ch:sda,s:e.s,e:Math.max(e.e,e.s+1),t,err:(e.k=='addr'||e.k=='data')&&!e.ack});
  if(e.k=='start')cur='S ';else if(e.k=='stop'){lines.push(cur+'P');cur=''}else cur+=t+' '});
 if(cur)lines.push(cur);$('itext').textContent=lines.slice(0,20).join('\n')||'(nothing decoded)';draw()}
$('dec').onclick=decode;
function trigStr(){let s='';for(let c=0;c<8;c++)s+=$('t'+c).value;return s}
async function capture(){if(demo){demoCapture();return}
 $('st').textContent='waiting for the trigger…';$('go').disabled=true;
 const q=new URLSearchParams({rate:$('rate').value,samples:$('samples').value,pre:$('pre').value,trigger:trigStr()});
 const r=await fetch('/api/capture?'+q);if(!r.ok){$('st').textContent=await r.text();$('go').disabled=false;return}
 for(;;){await new Promise(z=>setTimeout(z,150));info=await(await fetch('/api/status',{cache:'no-store'})).json();if(!info.busy)break}
 data=new Uint8Array(await(await fetch('/api/data',{cache:'no-store'})).arrayBuffer());$('go').disabled=false;showResult()}
$('go').onclick=capture;$('abort').onclick=()=>{if(!demo)fetch('/api/abort')};
$('vcd').onclick=()=>{if(!data.length)return;const ps=Math.round(1e12/info.actual_rate);let v='$timescale 1ps $end\n$scope module la $end\n';
 for(let c=0;c<8;c++)v+='$var wire 1 '+String.fromCharCode(33+c)+' ch'+c+'_gpio'+PINS[c]+' $end\n';v+='$upscope $end\n$enddefinitions $end\n';
 let prev=-1;for(let i=0;i<data.length;i++){if(data[i]===prev)continue;v+='#'+i*ps+'\n';for(let c=0;c<8;c++)if(prev<0||((data[i]^prev)>>c)&1)v+=((data[i]>>c)&1)+String.fromCharCode(33+c)+'\n';prev=data[i]}
 const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([v],{type:'text/plain'}));a.download='capture.vcd';a.click()};
/* ---- demo: UART "Hello" on ch0, 10 kHz PWM on ch1, I2C on ch2/3, a counter on ch4..7 ---- */
let demoAnn={uart:[],i2c:[]};
function demoCapture(){const rate=1e6,n=16384,d=new Uint8Array(n).fill(0x0D);demoAnn={uart:[],i2c:[]};
 const put=(c,i,v)=>{if(i<n)d[i]=v?d[i]|(1<<c):d[i]&~(1<<c)};
 const spb=rate/115200;let at=1638;for(const ch of 'Hello from ESP32 7\n'){const b=ch.charCodeAt(0),bits=[0,...[0,1,2,3,4,5,6,7].map(k=>(b>>k)&1),1];
  bits.forEach((v,k)=>{for(let i=Math.round(at+k*spb);i<Math.round(at+(k+1)*spb);i++)put(0,i,v)});demoAnn.uart.push({s:at,e:Math.round(at+9.5*spb),v:b});at+=Math.round(10*spb)+3}
 for(let i=0;i<n;i++){put(1,i,(i%100)<30);put(4,i,(i>>9)&1);put(5,i,(i>>10)&1);put(6,i,(i>>11)&1);put(7,i,(i>>12)&1)}
 let p=4200;const half=5,ph=(sda,scl,k)=>{for(let j=0;j<k;j++){put(2,p,sda);put(3,p,scl);p++}};
 const bit=v=>{ph(v,0,half);ph(v,1,half)};
 ph(1,1,10);const s0=p;ph(0,1,half);demoAnn.i2c.push({k:'start',s:s0,e:s0});
 const byte=(v,ack,kind,extra)=>{const s=p;for(let k=7;k>=0;k--)bit((v>>k)&1);bit(ack?0:1);demoAnn.i2c.push(Object.assign({k:kind,s,e:p-half,v:kind=='addr'?v>>1:v,ack:ack?1:0},extra))};
 byte(0x3C<<1,0,'addr',{rd:0});ph(0,0,half);ph(0,1,half);const s1=p;ph(1,1,20);demoAnn.i2c.push({k:'stop',s:s1,e:s1});
 data=d;info={actual_rate:rate,trigger_index:1637,triggered:true,irq_off:true,max_late_ns:96};showResult()}
draw();if(demo){demoCapture();const v=new URLSearchParams(location.search).get('view');
 if(v){[x0,x1]=v.split('-').map(Number);clampView();draw()}}
</script></body></html>)HTML";

#endif /* WEB_UI_H */
