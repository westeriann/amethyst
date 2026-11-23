// Phone remote served by the built-in web server. It builds its controls from /actions,
// so every action in the app is available, grouped like the desktop UI.
#pragma once

static const char* kRemotePage = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="theme-color" content="#0b0a12">
<title>Amethyst Remote</title>
<style>
:root{--bg:#0b0a12;--panel:#15131f;--line:#2a2638;--text:#ecebf5;--dim:#9a98ad;--a:#b388ff;--b:#4ee6c8}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.35 system-ui,-apple-system,"Segoe UI",sans-serif;padding:12px 12px 40px}
header{position:sticky;top:0;background:var(--bg);padding:6px 0 10px;z-index:2}
#now{font-weight:650;font-size:17px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#sub{color:var(--dim);font-size:13px;display:flex;gap:10px;flex-wrap:wrap}
#beat{display:inline-flex;gap:4px;vertical-align:middle}#beat i{width:9px;height:9px;border-radius:50%;background:var(--line)}#beat i.on{background:#ff6ec7}
h2{font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:var(--a);margin:18px 2px 8px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(96px,1fr));gap:8px}
button{font:inherit;color:var(--text);background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:14px 8px;min-height:54px;touch-action:manipulation}
button:active{background:#2b2442;border-color:var(--a)}
button.on{border-color:var(--b);background:rgba(78,230,200,.16)}
.slider{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:10px 12px;grid-column:1/-1}
.slider label{display:flex;justify-content:space-between;color:var(--dim);font-size:13px}
input[type=range]{width:100%;height:34px;accent-color:var(--a)}
#err{color:#ff8fa3;font-size:13px}
</style></head><body>
<header><div id="now">Connecting…</div><div id="sub"><span id="bpm"></span><span id="beat"><i></i><i></i><i></i><i></i></span><span id="err"></span></div></header>
<main id="app"></main>
<script>
const app=document.getElementById('app');let actions=[];const sliders={},toggles={};
const send=(id,v)=>fetch('/a/'+encodeURIComponent(id)+(v===undefined?'':'?v='+v),{method:'POST'}).catch(()=>{});
async function build(){
  actions=await (await fetch('/actions')).json();const groups={};
  for(const a of actions)(groups[a.group]=groups[a.group]||[]).push(a);
  for(const g of Object.keys(groups)){
    const h=document.createElement('h2');h.textContent=g;app.appendChild(h);
    const grid=document.createElement('div');grid.className='grid';app.appendChild(grid);
    for(const a of groups[g]){
      if(a.kind==='continuous'){
        const w=document.createElement('div');w.className='slider';
        w.innerHTML='<label><span></span><b></b></label><input type="range" min="0" max="1000">';
        w.querySelector('span').textContent=a.label;const r=w.querySelector('input'),b=w.querySelector('b');
        let t=0;r.oninput=()=>{b.textContent=(r.value/1000).toFixed(2);clearTimeout(t);t=setTimeout(()=>send(a.id,r.value/1000),25)};
        sliders[a.id]={r,b,busy:()=>document.activeElement===r};grid.appendChild(w);
      }else{
        const bt=document.createElement('button');bt.textContent=a.label;
        if(a.kind==='momentary'){bt.onpointerdown=()=>send(a.id,1);bt.onpointerup=bt.onpointerleave=()=>send(a.id,0);}
        else bt.onclick=()=>send(a.id);
        if(a.kind==='toggle')toggles[a.id]=bt;grid.appendChild(bt);
      }
    }
  }
}
async function poll(){
  try{const s=await (await fetch('/state')).json();
    document.getElementById('now').textContent=s.a+(s.x>0.02?'  ⇄  '+s.b:'');
    document.getElementById('bpm').textContent=s.bpm.toFixed(1)+' BPM · '+s.src;
    document.querySelectorAll('#beat i').forEach((e,i)=>e.classList.toggle('on',i===s.beat));
    for(const [id,v] of Object.entries(s.values)){if(sliders[id]&&!sliders[id].busy()){sliders[id].r.value=v*1000;sliders[id].b.textContent=v.toFixed(2)}
      if(toggles[id])toggles[id].classList.toggle('on',v>0.5)}
    document.getElementById('err').textContent='';
  }catch(e){document.getElementById('err').textContent='offline'}
  setTimeout(poll,350);
}
build().then(poll);
</script></body></html>)HTML";
