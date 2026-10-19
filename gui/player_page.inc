// The page the players' phones load: HTML, CSS and JavaScript in one. It polls
// /state (JSON published by the app) and shows the player's own character big,
// the turn order and whose turn it is, with the HP animations. Read-only.
// Split in parts because some compilers limit the length of one string literal.
static const char* const PLAYER_PAGE_PARTS[] = {
R"PAGE(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#ececE8">
<title>HP Tracker</title>
<style>
:root{--ink:#1a1a1c;--bg:#ecece8;--panel:#fff;--muted:#86867f;--track:#ececE8;--green:#2ea44f;--amber:#e69614;--red:#e2483c;--temp:#0ea0e1;--select:#fab800}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--ink);font-family:-apple-system,"Helvetica Neue",Helvetica,Arial,sans-serif}
body{padding:max(14px,env(safe-area-inset-top)) 16px max(28px,env(safe-area-inset-bottom));max-width:560px;margin:0 auto}
.heavy{font-family:"Arial Black","Helvetica Neue",Arial,sans-serif;font-weight:900}
header{display:flex;align-items:center;justify-content:space-between;gap:10px;margin:4px 2px 16px}
header h1{font-size:25px;margin:0;letter-spacing:-.5px}
.pill{display:inline-flex;align-items:center;gap:7px;background:var(--panel);border:2px solid var(--ink);border-radius:999px;padding:4px 12px;font-size:14px;font-weight:700;white-space:nowrap}
.dot{width:9px;height:9px;border-radius:50%;background:var(--green)}
.dot.off{background:var(--red);animation:blink 1s infinite}
.box{background:var(--panel);border:2px solid var(--ink);border-radius:18px;box-shadow:4px 5px 0 var(--ink);position:relative}
#banner{position:fixed;left:16px;right:16px;top:max(12px,env(safe-area-inset-top));z-index:9;background:var(--ink);color:#fff;border-radius:16px;padding:16px;text-align:center;font-size:22px;box-shadow:4px 5px 0 var(--select);transform:translateY(-160%);transition:transform .45s cubic-bezier(.2,1.4,.4,1)}
#banner.show{transform:none}
#who,#wait{padding:18px;margin-bottom:24px}
.pick{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:14px}
.pick button{display:flex;align-items:center;gap:8px;font:inherit;font-size:16px;font-weight:700;text-align:left;background:#fff;color:var(--ink);border:2px solid var(--ink);border-radius:14px;padding:10px;box-shadow:3px 4px 0 var(--ink)}
.pick button:active{transform:translate(2px,3px);box-shadow:1px 1px 0 var(--ink)}
.pick svg{width:22px;height:22px;flex:none}
#me{padding:18px;margin-bottom:24px;overflow:hidden;transition:box-shadow .3s}
#me.turn{box-shadow:4px 5px 0 var(--ink),0 0 0 4px var(--select)}
#me.dying{background:#fff1ef;animation:beat 1.6s infinite}
#me.dead{background:#f1f1ee}
.head{display:flex;align-items:center;gap:12px}
.tile{width:56px;height:56px;border-radius:15px;display:flex;align-items:center;justify-content:center;flex:none}
.tile svg{width:40px;height:40px}
.name{font-size:25px;line-height:1.08;word-break:break-word}
#me.dead .name{text-decoration:line-through;color:var(--muted)}
.sub{color:var(--muted);font-size:14px;margin-top:3px}
.turnTag{margin-left:auto;align-self:flex-start;background:var(--ink);color:#fff;border-radius:999px;padding:5px 11px;font-size:12px;font-weight:800;display:none;white-space:nowrap}
#me.turn .turnTag{display:inline-block;animation:pop .35s}
.hpline{display:flex;align-items:baseline;gap:6px;margin:16px 0 9px;flex-wrap:wrap}
.hp{font-size:56px;line-height:1}
.max{font-size:21px;color:var(--muted)}
.temp{font-size:18px;color:var(--temp);font-weight:800}
.bar{position:relative;height:20px;background:var(--track);border:2px solid var(--ink);border-radius:999px;overflow:hidden}
.bar .chunk,.bar .fill{position:absolute;left:0;top:0;bottom:0;width:0}
.bar .chunk{background:#ffd27a}
.bar .fill{background:var(--green)}
.tempbar{height:6px;margin:4px 3px 0;background:var(--temp);border-radius:3px;width:0;transition:width .4s}
.saves{display:none;align-items:center;gap:8px;margin-top:14px;font-weight:700;flex-wrap:wrap}
.saves .sep{width:8px}
.pip{width:21px;height:21px;border-radius:50%;border:2px solid #c0c0ba;background:#fff;transition:background .3s}
.pip.ok{background:var(--green);border-color:var(--ink)}
.pip.bad{background:var(--red);border-color:var(--ink)}
.stats{display:flex;flex-wrap:wrap;gap:8px;margin-top:16px}
.stat{border:1.5px solid #d6d6d0;border-radius:12px;padding:6px 11px;min-width:66px;text-align:center}
.stat b{display:block;font-size:20px}
.stat span{font-size:12px;color:var(--muted)}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:12px}
.chip{font-size:12px;font-weight:800;border-radius:999px;padding:3px 9px;border:1.5px solid}
.float{position:absolute;right:20px;top:92px;font-size:38px;pointer-events:none;animation:rise 1.15s ease-out forwards;-webkit-text-stroke:1.5px var(--ink);z-index:3}
.float.dmg{color:var(--red)}.float.heal{color:var(--green)}.float.tmp{color:var(--temp);font-size:28px}
.float.crit{font-size:60px}
.shake{animation:shake .35s}.shake.big{animation:shake .7s}
.flash{position:absolute;inset:0;border-radius:16px;pointer-events:none;animation:flash .55s forwards;z-index:2}
.stamp{position:absolute;inset:0;display:none;align-items:center;justify-content:center;font-size:46px;color:var(--red);transform:rotate(-12deg);-webkit-text-stroke:2px var(--ink);z-index:4;pointer-events:none}
#me.dead .stamp{display:flex;animation:stamp .5s cubic-bezier(.2,1.6,.4,1) both}
h2{font-size:17px;margin:0 2px 12px;display:flex;justify-content:space-between;align-items:baseline}
h2 .sub{font-family:-apple-system,"Helvetica Neue",Helvetica,Arial,sans-serif;font-weight:700}
#order{display:flex;flex-direction:column;gap:11px;padding-left:12px}
.row{display:flex;align-items:center;gap:10px;padding:9px 12px;background:#fff;border:2px solid var(--ink);border-radius:14px;box-shadow:3px 4px 0 var(--ink);position:relative;transition:transform .35s cubic-bezier(.2,1.3,.4,1),opacity .4s}
.row .t{width:36px;height:36px;border-radius:10px;flex:none;display:flex;align-items:center;justify-content:center}
.row svg{width:25px;height:25px}
.row .n{flex:1;min-width:0}
.row .nm{font-size:16px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.row .mini{height:8px;background:var(--track);border:1.5px solid var(--ink);border-radius:99px;overflow:hidden;margin-top:6px}
.row .mini i{display:block;height:100%;width:0;background:var(--green);transition:width .5s .25s,background .3s}
.row .hpt{font-size:15px;font-weight:800;white-space:nowrap}
.row .kind{font-size:12px;color:var(--muted);margin-top:3px}
.row.turn{transform:translateX(10px);box-shadow:3px 4px 0 var(--ink),0 0 0 3px var(--select)}
.row.turn::before{content:"\25B6";position:absolute;left:-22px;font-size:13px}
.row.mine{background:#fffbe8}
.row.dying{background:#fff1ef}
.row.dead{opacity:.5}.row.dead .nm{text-decoration:line-through}
.row.hit{animation:shake .35s}
.empty{color:var(--muted);text-align:center;padding:18px 0}
footer{text-align:center;margin-top:26px}
footer button{font:inherit;font-size:14px;font-weight:700;color:var(--muted);background:none;border:none;text-decoration:underline}
@keyframes rise{0%{transform:translateY(8px) scale(.6);opacity:0}12%{transform:translateY(0) scale(1.08);opacity:1}60%{opacity:1}100%{transform:translateY(-60px) scale(1);opacity:0}}
@keyframes shake{0%,100%{transform:none}20%{transform:translate(-6px,2px)}40%{transform:translate(5px,-2px)}60%{transform:translate(-4px,1px)}80%{transform:translate(3px,0)}}
@keyframes flash{from{opacity:.55}to{opacity:0}}
@keyframes stamp{from{transform:rotate(-12deg) scale(2.4);opacity:0}to{transform:rotate(-12deg) scale(1);opacity:1}}
@keyframes pop{0%{transform:scale(.5)}70%{transform:scale(1.12)}100%{transform:scale(1)}}
@keyframes beat{0%,100%{background:#fff4f2}50%{background:#ffe2de}}
@keyframes blink{50%{opacity:.25}}
</style></head><body>
<div id="banner" class="heavy">&#9654; <span data-tr="It's your turn!">It's your turn!</span></div>
<header><h1 class="heavy">HP Tracker</h1><span class="pill"><span id="live" class="dot off"></span><span id="status" data-tr="Connecting...">Connecting...</span></span></header>
<div id="wait" class="box" hidden><div class="heavy" style="font-size:20px" data-tr="Waiting for the party">Waiting for the party</div><div class="sub" data-tr="The DM hasn't added any players yet.">The DM hasn't added any players yet.</div></div>
<div id="who" class="box" hidden><div class="heavy" style="font-size:20px" data-tr="Who are you?">Who are you?</div><div class="sub" data-tr="Tap your character. This phone will remember it.">Tap your character. This phone will remember it.</div><div id="pick" class="pick"></div></div>
<div id="me" class="box" hidden>
<div class="head"><div class="tile" id="meTile"></div><div style="min-width:0"><div class="name heavy" id="meName"></div><div class="sub" id="meSub"></div></div><span class="turnTag">&#9654; <span data-tr="YOUR TURN">YOUR TURN</span></span></div>
<div class="hpline"><span class="hp heavy" id="meHp"></span><span class="max" id="meMax"></span><span class="temp" id="meTemp"></span></div>
<div class="bar"><div class="chunk" id="meChunk"></div><div class="fill" id="meFill"></div></div>
<div class="tempbar" id="meTempBar"></div>
<div class="saves" id="meSaves"><span data-tr="Death saves">Death saves</span> <span class="pip"></span><span class="pip"></span><span class="pip"></span><span class="sep"></span><span class="pip"></span><span class="pip"></span><span class="pip"></span></div>
<div class="stats" id="meStats"></div>
<div class="chips" id="meChips"></div>
<div class="stamp heavy" data-tr="DEAD">DEAD</div>
</div>
<h2 class="heavy"><span id="orderTitle">Turn order</span><span id="round" class="sub"></span></h2>
<div id="order"></div>
<footer><button id="change" hidden data-tr="Change character">Change character</button></footer>
)PAGE",
R"PAGE(<script>
"use strict";
const $=id=>document.getElementById(id);
// The app's icons, in a 0..1 box (same drawings as on the cards).
const L=(a,b,c,d)=>`<line x1="${a}" y1="${b}" x2="${c}" y2="${d}"/>`;
const C=(x,y,r,f)=>`<circle cx="${x}" cy="${y}" r="${r}"${f?' fill="currentColor" stroke="none"':''}/>`;
const PG=p=>`<polygon points="${p}"/>`;
const star=(()=>{let p=[];for(let i=0;i<10;i++){const r=i%2?.18:.42,a=-Math.PI/2+i*Math.PI/5;p.push((.5+Math.cos(a)*r).toFixed(3)+','+(.54+Math.sin(a)*r).toFixed(3))}return PG(p.join(' '))})();
const ICONS=[
'<path d="M.5 .88C.02 .58 .08 .12 .5 .32C.92 .12 .98 .58 .5 .88Z"/>',
PG('.6,.08 .26,.56 .5,.56 .4,.92 .76,.42 .52,.42'),
PG('.18,.14 .82,.14 .82,.48 .5,.9 .18,.48')+L(.5,.26,.5,.72),
L(.3,.7,.84,.16)+L(.2,.56,.44,.8)+L(.3,.7,.13,.87),
C(.5,.42,.3)+'<rect x=".36" y=".64" width=".28" height=".22" rx=".06"/>'+C(.39,.44,.075,1)+C(.61,.44,.075,1),
star,
'<path d="M.5 .08C.7 .3 .86 .5 .7 .76C.62 .92 .38 .92 .3 .76C.14 .5 .3 .3 .5 .08Z"/><path d="M.5 .5C.62 .62 .6 .8 .5 .82C.4 .8 .38 .62 .5 .5Z" stroke-width=".06"/>',
C(.5,.66,.17)+C(.22,.42,.08)+C(.39,.25,.08)+C(.61,.25,.08)+C(.78,.42,.08),
PG('.5,.07 .87,.29 .87,.71 .5,.93 .13,.71 .13,.29')+PG('.5,.3 .71,.64 .29,.64')+L(.5,.3,.5,.07)+L(.71,.64,.87,.71)+L(.29,.64,.13,.71),
PG('.13,.76 .13,.3 .33,.52 .5,.22 .67,.52 .87,.3 .87,.76'),
'<path d="M.6725 .8386A.38 .38 0 1 1 .6725 .1614L.6131 .1637A.3 .3 0 0 0 .6131 .7563Z"/>',
'<path d="M.08 .5C.32 .16 .68 .16 .92 .5C.68 .84 .32 .84 .08 .5Z"/>'+C(.5,.5,.17)+C(.5,.5,.08,1)];
const CROSS=L(.25,.25,.75,.75)+L(.75,.25,.25,.75);
const svg=(i,col)=>`<svg viewBox="0 0 1 1" fill="none" stroke="${col}" style="color:${col}" stroke-width=".075" stroke-linecap="round" stroke-linejoin="round">${i<0?CROSS:(ICONS[i]||ICONS[0])}</svg>`;
const tint=(hex,a)=>hex+Math.round(a*255).toString(16).padStart(2,'0');
const hpColor=f=>f>.5?'var(--green)':f>.25?'var(--amber)':'var(--red)';
// The page follows the app's language (sent as s.lang); the texts it writes itself are
// translated here, the ones from the fight (conditions, damage type) come translated.
const TR={
es:{"It's your turn!":"¡Es tu turno!","Connecting...":"Conectando...","Reconnecting...":"Reconectando...","Waiting for the party":"Esperando al grupo","The DM hasn't added any players yet.":"El máster aún no ha añadido jugadores.","Who are you?":"¿Quién eres?","Tap your character. This phone will remember it.":"Toca tu personaje. Este móvil lo recordará.","YOUR TURN":"TU TURNO","Death saves":"Salvaciones de muerte","DEAD":"MUERTO","Change character":"Cambiar de personaje","Level":"Nivel","HP":"PG","temp":"temp","AC":"CA","Speed":"Velocidad","Passive Perc.":"Percep. pasiva","Damage":"Daño","DYING":"MURIENDO","STABLE":"ESTABLE","BLOODIED":"MALHERIDO","CONC":"CONC","Turn order":"Orden de turno","The party":"El grupo","Round":"Ronda","No one here yet.":"Aún no hay nadie.","Enemy":"Enemigo","Setting up":"Preparando"},
ca:{"It's your turn!":"És el teu torn!","Connecting...":"Connectant...","Reconnecting...":"Reconnectant...","Waiting for the party":"Esperant el grup","The DM hasn't added any players yet.":"El màster encara no ha afegit jugadors.","Who are you?":"Qui ets?","Tap your character. This phone will remember it.":"Toca el teu personatge. Aquest mòbil el recordarà.","YOUR TURN":"EL TEU TORN","Death saves":"Salvacions de mort","DEAD":"MORT","Change character":"Canviar de personatge","Level":"Nivell","HP":"PV","temp":"temp","AC":"CA","Speed":"Velocitat","Passive Perc.":"Percep. passiva","Damage":"Dany","DYING":"MORINT","STABLE":"ESTABLE","BLOODIED":"MALFERIT","CONC":"CONC","Turn order":"Ordre de torn","The party":"El grup","Round":"Ronda","No one here yet.":"Encara no hi ha ningú.","Enemy":"Enemic","Setting up":"Preparant"}};
let lang='en';
const tr=k=>(TR[lang]&&TR[lang][k])||k;
function setLang(l){if(l===lang)return;lang=l;document.documentElement.lang=l;document.querySelectorAll('[data-tr]').forEach(e=>e.textContent=tr(e.dataset.tr));}
const esc=t=>String(t).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));

let me=localStorage.getItem('hpTrackerMe')||'';
{const q=new URLSearchParams(location.search).get('me');if(q){me=q;localStorage.setItem('hpTrackerMe',me);}}  // ?me=Name picks the character
let last=null,prev={},prevTurn=null,first=true,rows={};

let reconnecting=false;
function setLive(on){$('live').className=on?'dot':'dot off';if(!on){reconnecting=true;$('status').textContent=tr('Reconnecting...');}}

function floatNum(text,kind,crit){const f=document.createElement('div');f.className='float heavy '+kind+(crit?' crit':'');f.textContent=text;$('me').appendChild(f);setTimeout(()=>f.remove(),1200);}
function flash(color){const f=document.createElement('div');f.className='flash';f.style.background=color;$('me').appendChild(f);setTimeout(()=>f.remove(),600);}
function shake(el,big){el.classList.remove('shake','big','hit');void el.offsetWidth;el.classList.add(el.id==='me'?'shake':'hit');if(big)el.classList.add('big');}
function setBar(fill,chunk,old,now,max){
  const w=v=>Math.max(0,Math.min(100,100*v/Math.max(1,max)))+'%';
  fill.style.background=hpColor(now/Math.max(1,max));
  if(old===undefined){fill.style.transition=chunk.style.transition='none';fill.style.width=chunk.style.width=w(now);void fill.offsetWidth;return;}
  if(now<old){chunk.style.background='#ffd27a';chunk.style.transition='width .5s ease .55s';fill.style.transition='width .18s ease';}
  else{chunk.style.background='#bff0c8';chunk.style.transition='width .18s ease';fill.style.transition='width .5s ease .55s';}
  fill.style.width=w(now);chunk.style.width=w(now);
}

function showMe(p,s){
  const box=$('me'),before=prev[p.name];
  box.hidden=false;
  const dead=p.life==='dead';
  $('meTile').style.background=tint(p.color,dead?.08:.16);
  $('meTile').innerHTML=svg(dead?-1:p.icon,dead?'#969692':p.color);
  $('meName').textContent=p.name;
  $('meSub').textContent=tr('Level')+' '+p.level;
  box.classList.toggle('turn',!!s.combat&&s.turn===p.name);
  box.classList.toggle('dying',p.life==='dying');
  box.classList.toggle('dead',dead);
  $('meHp').textContent=p.hp;$('meMax').textContent='/ '+p.max+' '+tr('HP');
  $('meTemp').textContent=p.temp>0?'+'+p.temp+' '+tr('temp'):'';
  setBar($('meFill'),$('meChunk'),before&&before.hp,p.hp,p.max);
  $('meTempBar').style.width=(p.temp>0?Math.max(4,Math.min(100,100*p.temp/p.max)):0)+'%';
  if(before){
    const d=p.hp-before.hp,crit=Math.abs(d)>=.4*p.max;
    if(d<0){floatNum(d,'dmg',crit);shake(box,crit);flash('rgba(255,60,60,.5)');if(navigator.vibrate)navigator.vibrate(crit?[90,40,140]:60);}
    else if(d>0){floatNum('+'+d,'heal',crit);flash('rgba(60,200,100,.45)');}
    else if(p.temp<before.temp){floatNum('-'+(before.temp-p.temp)+' '+tr('temp'),'tmp');shake(box,false);}
    else if(p.temp>before.temp){floatNum('+'+(p.temp-before.temp)+' '+tr('temp'),'tmp');}
  }
  const sv=$('meSaves');sv.style.display=(p.hp===0&&!dead)?'flex':'none';
  const pips=sv.querySelectorAll('.pip');
  pips.forEach((e,i)=>{e.className='pip'+(i<3?(i<p.saves[0]?' ok':''):(i-3<p.saves[1]?' bad':''));});
  let st=`<div class="stat"><b>${p.ac}</b><span>${tr('AC')}</span></div><div class="stat"><b>${p.speed}</b><span>${tr('Speed')}</span></div><div class="stat"><b>${p.pp}</b><span>${tr('Passive Perc.')}</span></div>`;
  if(p.mod)st+=`<div class="stat"><b style="font-size:15px;line-height:24px">${esc(p.mod)}</b><span>${tr('Damage')}</span></div>`;
  $('meStats').innerHTML=st;
  const chip=(t,c)=>`<span class="chip" style="color:${c};border-color:${c};background:${tint(c,.1)}">${esc(t)}</span>`;
  let ch='';
  if(p.life==='dying')ch+=chip(tr('DYING'),'#e2483c');if(p.life==='stable')ch+=chip(tr('STABLE'),'#2ea44f');
  if(p.bloodied)ch+=chip(tr('BLOODIED'),'#e2483c');if(p.conc)ch+=chip(tr('CONC'),'#8b5cf6');
  for(const c of p.conditions)ch+=chip(c[0]+(c[1]>0?' '+c[1]:''),'#b07a04');
  $('meChips').innerHTML=ch;
}

function renderOrder(s){
  const box=$('order'),seen={};
  $('orderTitle').textContent=tr(s.combat?'Turn order':'The party');
  $('round').textContent=s.combat?tr('Round')+' '+s.round:'';
  if(!s.order.length){box.innerHTML='<div class="empty">'+tr('No one here yet.')+'</div>';rows={};return;}
  const empty=box.querySelector('.empty');if(empty)empty.remove();
  for(const o of s.order){
    let r=rows[o.name];
    if(!r){r=document.createElement('div');r.className='row';r.innerHTML='<div class="t"></div><div class="n"><div class="nm heavy"></div><div class="mini"><i></i></div><div class="kind"></div></div><div class="hpt"></div>';rows[o.name]=r;}
    seen[o.name]=1;
    const player=o.side==='player',dead=o.life==='dead';
    r.classList.toggle('turn',!!s.combat&&s.turn===o.name);
    r.classList.toggle('mine',o.name===me);
    r.classList.toggle('dying',o.life==='dying');
    r.classList.toggle('dead',dead);
    r.querySelector('.t').style.background=tint(o.color,.15);
    r.querySelector('.t').innerHTML=svg(dead?-1:o.icon,dead?'#969692':o.color);
    r.querySelector('.nm').textContent=o.name;
    r.querySelector('.mini').style.display=player?'block':'none';
    r.querySelector('.kind').textContent=player?'':tr('Enemy');
    r.querySelector('.hpt').textContent=player?o.hp+' / '+o.max:'';
    if(player){const i=r.querySelector('.mini i');i.style.width=Math.max(0,Math.min(100,100*o.hp/Math.max(1,o.max)))+'%';i.style.background=hpColor(o.hp/Math.max(1,o.max));
      const b=prev[o.name];if(b&&o.hp<b.hp&&o.name!==me)shake(r,false);}
    box.appendChild(r);
  }
  for(const n in rows)if(!seen[n]){rows[n].remove();delete rows[n];}
}

function render(s){
  if(s.lang)setLang(s.lang);
  $('status').textContent=s.combat?tr('Round')+' '+s.round:tr('Setting up');
  const players=s.order.filter(o=>o.side==='player');
  const mine=players.find(p=>p.name===me);
  $('wait').hidden=players.length>0;
  $('who').hidden=!(players.length>0&&!mine);
  $('change').hidden=!mine;
  if(!mine){
    $('me').hidden=true;
    $('pick').innerHTML=players.map(p=>`<button data-n="${esc(p.name)}">${svg(p.icon,p.color)}<span>${esc(p.name)}</span></button>`).join('');
    $('pick').querySelectorAll('button').forEach(b=>b.onclick=()=>{me=b.dataset.n;localStorage.setItem('hpTrackerMe',me);prev={};render(s);});
  } else showMe(mine,s);
  renderOrder(s);
  if(s.combat&&me&&s.turn===me&&prevTurn!==me&&!first){const b=$('banner');b.classList.add('show');if(navigator.vibrate)navigator.vibrate([120,60,120]);setTimeout(()=>b.classList.remove('show'),2600);}
  prevTurn=s.turn;first=false;
  prev={};for(const p of players)prev[p.name]={hp:p.hp,temp:p.temp,life:p.life};
}

$('change').onclick=()=>{me='';localStorage.removeItem('hpTrackerMe');prev={};if(last)render(last);};

async function tick(){
  try{
    const r=await fetch('state',{cache:'no-store'});
    const s=await r.json();
    $('live').className='dot';
    if(!last||s.v!==last.v||reconnecting){reconnecting=false;render(s);}
    last=s;
  }catch(e){setLive(false);}
  setTimeout(tick,400);
}
tick();
</script>
</body></html>
)PAGE",
};
