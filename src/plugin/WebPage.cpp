// SPDX-License-Identifier: GPL-2.0-or-later
#include "WebPage.hpp"

// The remote page: one self-contained document (no libraries, no external requests) that draws the
// dock again in the browser, with the same colors, the same arrangement of the controls and the
// same progress border around the clock, and polls /api/state. With the control link it posts the
// commands the dock sends. The text comes from the plugin's locale files through the __I18N__
// object, so the page speaks the language of OBS; the clock digits are the plugin's own font,
// served at /font. Adjacent literals stay under the 16 KB limit of one MSVC string.

namespace {

constexpr const char* kStyle = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="robots" content="noindex"><title>ScoreBoard</title>
<style>
:root{--win:#1d1f26;--card:#272a33;--inset:#15171c;--btn:#3c404d;--btn-hi:#4b5060;--bdr:#383b46;
--txt:#fff;--txt2:#d6d9e0;--mut:#9aa0ae;--prim:#284cb8;--home:#284cb8;--away:#ef7d57;
--go:#25a231;--stop:#c01c37;--pen:#ef7d57;--h:26px;--hs:20px;--fs:12px}
@media(prefers-color-scheme:light){:root{--win:#e9eaee;--card:#f3f4f7;--inset:#fff;--btn:#d9dce4;
--btn-hi:#cbd0db;--bdr:#c3c7d2;--txt:#14161c;--txt2:#2c303b;--mut:#626878;--go:#2e9d3a;--stop:#cf2b45}}
@media(pointer:coarse){:root{--h:42px;--hs:32px;--fs:15px}}
*{box-sizing:border-box}html,body{margin:0}
body{background:var(--win);color:var(--txt);font:var(--fs)/1.3 system-ui,-apple-system,"Segoe UI",sans-serif;
padding:8px max(8px,env(safe-area-inset-right)) 16px max(8px,env(safe-area-inset-left));-webkit-text-size-adjust:100%}
main{max-width:520px;margin:0 auto;display:grid;grid-template-columns:minmax(0,1fr);gap:6px}
[hidden]{display:none!important}
button{font:inherit;font-weight:600;color:var(--txt);background:var(--btn);border:1px solid var(--btn);border-radius:4px;
height:var(--h);padding:0 10px;touch-action:manipulation;cursor:pointer;display:inline-flex;align-items:center;
justify-content:center;gap:6px;min-width:0}
button:hover{background:var(--btn-hi)}button:active{filter:brightness(.85)}button:disabled{opacity:.45;cursor:default}
button svg{width:14px;height:14px;flex:none}
.ghost{background:transparent;border-color:var(--bdr);font-weight:500}.ghost:hover{background:var(--btn)}
.go,.stop{height:calc(var(--h) + 6px);font-size:1.08em;color:#fff}
.go{background:var(--go);border-color:var(--go)}.stop{background:var(--stop);border-color:var(--stop)}
.big{height:calc(var(--h) + 6px)}
.seg{display:flex}.seg>*{border-radius:0;border-left-width:0}
.seg>*:first-child{border-left-width:1px;border-radius:4px 0 0 4px}.seg>*:last-child{border-radius:0 4px 4px 0}
.seg>button{flex:0 0 calc(var(--h) + 8px);padding:0}
.seg.sm>button{flex-basis:var(--h)}
.val{flex:1;display:flex;align-items:center;justify-content:center;background:var(--inset);border:1px solid var(--bdr);
border-left-width:0;font-weight:700;height:var(--h);min-width:34px;padding:0 6px}
.seg>.val:first-child{border-left-width:1px}
.seg.large .val{height:calc(var(--h) + 6px);font-size:1.7em}.seg.large>button{height:calc(var(--h) + 6px)}
.seg2{display:flex;margin-top:2px}.seg2>button{flex:1;border-radius:0;border-left-width:0;height:var(--h)}
.seg2>button:first-child{border-left-width:1px;border-radius:4px 0 0 4px}.seg2>button:last-child{border-radius:0 4px 4px 0}
.seg2>button:only-child{border-radius:4px}
.clockcard{position:relative;background:var(--inset);border:1px solid var(--bdr);border-radius:6px;padding:10px 8px 8px}
.clockcard svg.prog{position:absolute;inset:0;width:100%;height:100%;pointer-events:none;overflow:visible}
.cl{display:grid;grid-template-columns:auto minmax(0,1fr) auto;gap:8px;align-items:stretch}
.cl .col{display:grid;gap:4px;align-content:center}.cl .col button{height:auto;min-height:var(--h);min-width:calc(var(--h) + 22px)}
#clock{grid-column:2;min-width:0;overflow:hidden;text-align:center;align-self:center;color:var(--txt)}
#clk{display:inline-block;font-family:DSEG7,ui-monospace,Consolas,monospace;font-weight:700;font-size:64px;line-height:1.15;
white-space:nowrap;font-variant-numeric:tabular-nums}
.sub{display:flex;justify-content:center;align-items:center;gap:8px;margin-top:6px;color:var(--mut)}
.row{display:flex;gap:6px}.row>.go,.row>.stop{flex:1}
.teams{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:6px}
.team{background:var(--card);border:1px solid var(--bdr);border-top:3px solid var(--home);border-radius:6px;padding:8px;min-width:0}
.team.away{border-top-color:var(--away)}
.name{width:100%;text-align:center;font:inherit;font-weight:700;font-size:1.08em;background:var(--inset);color:var(--txt);
border:1px solid var(--bdr);border-radius:4px;height:calc(var(--h) - 2px);padding:0 6px;min-width:0}
.cap{color:var(--mut);font-size:.82em;font-weight:600;text-transform:uppercase;letter-spacing:.04em;text-align:center;margin:8px 0 3px}
.penhead{display:flex;align-items:center;justify-content:space-between;margin:8px 0 3px}
.penhead .cap{margin:0}
.pen{position:relative;display:flex;align-items:center;gap:4px;background:var(--inset);border:1px solid var(--bdr);
border-radius:4px;padding:2px 2px 5px 8px;margin-top:4px;font-weight:700;overflow:hidden}
.pen span{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.pen.q span{color:var(--mut);font-weight:400}
.pen button{height:var(--hs);width:var(--hs);padding:0;flex:none}
.pen i{position:absolute;left:0;bottom:0;height:3px;background:var(--pen)}
.pill{justify-self:center;border:1px solid var(--bdr);border-radius:10px;padding:1px 10px;background:var(--card);font-size:.92em;color:var(--txt2)}
.foot{display:flex;align-items:center;gap:8px;background:var(--card);border:1px solid var(--bdr);border-radius:6px;padding:4px 4px 4px 8px}
.foot b{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.badge{border:1px solid var(--bdr);border-radius:10px;padding:1px 9px;color:var(--mut);font-size:.9em}
.badge.live{color:#3fb950;border-color:#3fb950}.badge.lost{color:var(--stop);border-color:var(--stop)}
.foot button{height:var(--hs);font-size:.9em;padding:0 9px}
.msg{text-align:center;color:var(--stop);min-height:1.4em}
</style></head><body><main>
<section class="clockcard" id="card">
 <svg class="prog" id="prog" aria-hidden="true"><path id="bar" fill="none" stroke-width="3"></path></svg>
 <div class="cl">
  <div class="col ctl" hidden><button data-c="ClockAdjust" data-a='{"deltaTenths":600}'>+1:00</button>
   <button data-c="ClockAdjust" data-a='{"deltaTenths":-600}'>-1:00</button></div>
  <div id="clock"><span id="clk">--:--</span></div>
  <div class="col ctl" hidden><button data-c="ClockAdjust" data-a='{"deltaTenths":10}'>+0:01</button>
   <button data-c="ClockAdjust" data-a='{"deltaTenths":-10}'>-0:01</button></div>
 </div>
 <div class="sub">
  <div class="seg sm"><button class="ctl" id="prev" data-c="PeriodPrev" hidden></button><div class="val" id="period"></div>
   <button class="ctl" id="next" data-c="PeriodNext" hidden></button></div>
  <span id="of"></span>
  <div class="seg sm" id="stoppage" hidden><button class="ctl" data-c="StoppageAdd" data-a='{"minutes":-1}' hidden>&minus;</button>
   <div class="val" id="stopval"></div><button class="ctl" data-c="StoppageAdd" data-a='{"minutes":1}' hidden>+</button></div>
 </div>
</section>
<div class="row ctl" hidden><button class="go" id="toggle" data-c="ClockToggle"></button>
 <button class="ghost big" id="reset" data-c="ClockReset"></button></div>
<div class="teams"><section class="team" id="home"></section><section class="team away" id="away"></section></div>
<div id="strength" class="pill" hidden></div>
<div id="msg" class="msg"></div>
<footer class="foot"><b id="sport"></b><span id="badge" class="badge"></span>
 <button class="ghost" id="vib" hidden></button></footer>
</main>
)HTML";

constexpr const char* kScript = R"HTML(<script>
"use strict";
var CONTROL=__CONTROL__, T=__I18N__, KEY=new URLSearchParams(location.search).get("k")||"";
var $=function(id){return document.getElementById(id)}, t=function(k){return T[k]||k};
var ICON={
 play:'<svg viewBox="0 0 16 16"><path d="M5 3l8 5-8 5z" fill="currentColor"/></svg>',
 pause:'<svg viewBox="0 0 16 16"><rect x="3.5" y="3" width="3" height="10" fill="currentColor"/><rect x="9.5" y="3" width="3" height="10" fill="currentColor"/></svg>',
 reset:'<svg viewBox="0 0 16 16"><path d="M13.1 6.9A5.2 5.2 0 1 0 13.2 9" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/><path d="M11.4 2.8L7.6 0.6v4.4z" fill="currentColor" transform="translate(1.6 .6)"/></svg>',
 prev:'<svg viewBox="0 0 16 16"><path d="M10 3L5 8l5 5" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>',
 next:'<svg viewBox="0 0 16 16"><path d="M6 3l5 5-5 5" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>',
 cross:'<svg viewBox="0 0 16 16"><path d="M4 4l8 8M12 4l-8 8" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/></svg>'};
var last={}, lost=0, msgTimer=0, timer=0, vibOn=false, canVib=("vibrate" in navigator);
try{vibOn=localStorage.getItem("sbVib")!=="0"}catch(e){vibOn=true}
function buzz(p){if(CONTROL&&canVib&&vibOn)try{navigator.vibrate(p)}catch(e){}}
function say(text){$("msg").textContent=text||"";clearTimeout(msgTimer);if(text)msgTimer=setTimeout(function(){say("")},4000)}
function api(path,body){
  return fetch(path+"?k="+encodeURIComponent(KEY),body===undefined?{cache:"no-store"}:
    {method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(body)}).then(function(r){return r.json()})}
function send(name,args){
  buzz(12);
  return api("/api/"+name,args||{}).then(function(r){
    if(r.result==="confirmation"){
      if(confirm(t("Reason."+r.reason))){return send(name,Object.assign({},args,{confirmed:true}))}
    }else if(r.result==="rejected"){say(t("Reason."+r.reason));buzz([40,40,40])}
    else if(r.result==="error"){say(r.error);buzz([40,40,40])}
    poll()}).catch(function(){say(t("Web.Lost"))})}
function el(tag,cls,text){var e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e}
function btn(html,cls,name,args){var b=el("button",cls||"");b.innerHTML=html;b.onclick=function(){send(name,args)};return b}
function lum(hex){var m=/^#?([0-9a-f]{6})$/i.exec(hex||"");if(!m)return 0;var n=parseInt(m[1],16);
  return (0.299*(n>>16)+0.587*((n>>8)&255)+0.114*(n&255))/255}
function tint(e,hex){e.style.background=hex;e.style.color=lum(hex)>0.6?"#111":"#fff"}

/* ---- a team column: built once, then only its texts change ---- */
function makeCounter(team,stat){
  var box=el("div"),cap=el("div","cap"),seg=el("div","seg"),val=el("div","val");
  var minus=btn("&minus;","","AddStat",{team:team,stat:stat,delta:-1});
  box.appendChild(cap);box.appendChild(seg);
  return {box:box,cap:cap,seg:seg,val:val,minus:minus,plusKey:"",extra:null,team:team,stat:stat}}
function fillCounter(c,caption,value,plus,color){
  c.cap.textContent=caption;c.val.textContent=String(value);
  if(color)tint(c.val,color);else{c.val.style.background="";c.val.style.color=""}
  var key=CONTROL+"|"+(plus||[1]).join(",");
  if(c.plusKey===key)return;
  c.plusKey=key;c.seg.innerHTML="";if(c.extra){c.extra.remove();c.extra=null}
  if(!CONTROL){c.seg.appendChild(c.val);return}
  var list=plus||[1];
  c.seg.appendChild(c.minus);c.seg.appendChild(c.val);
  c.seg.appendChild(btn("+"+list[0],"","AddStat",{team:c.team,stat:c.stat,delta:list[0]}));
  if(list.length>1){c.extra=el("div","seg2");
    list.slice(1).forEach(function(n){c.extra.appendChild(btn("+"+n,"","AddStat",{team:c.team,stat:c.stat,delta:n}))});
    c.box.appendChild(c.extra)}}
function makeTeam(side){
  var root=$(side),T0={};
  T0.name=el("input","name");T0.name.type="text";T0.name.maxLength=40;T0.name.readOnly=!CONTROL;
  T0.name.onchange=function(){send("SetTeamName",{team:side,name:T0.name.value})};
  T0.score=makeCounter(side,"score");T0.shots=makeCounter(side,"shots");
  T0.fouls=makeCounter(side,"fouls");T0.fouls2=makeCounter(side,"fouls2");
  T0.score.seg.className="seg large";
  T0.penHead=el("div","penhead");T0.penCap=el("div","cap");T0.penHead.appendChild(T0.penCap);
  T0.penList=el("div");T0.penKey="";
  [T0.name,T0.score.box,T0.shots.box,T0.fouls.box,T0.fouls2.box,T0.penHead,T0.penList].forEach(function(n){root.appendChild(n)});
  return T0}
var TEAM={};
function paintTeam(side,s){
  var T0=TEAM[side],L=s.layout,K=s.look;
  if(document.activeElement!==T0.name)T0.name.value=s[side+"Name"];
  fillCounter(T0.score,t("Panel.Score."+L.scoreLabel),s[side+"Score"],L.scoreValues,K.scoreColor);
  T0.score.seg.className="seg large";
  T0.shots.box.hidden=!L.shots;
  if(L.shots)fillCounter(T0.shots,t("Panel.Shots"),s[side+"Shots"],null,"");
  T0.fouls.box.hidden=!L.fouls;
  if(L.fouls)fillCounter(T0.fouls,t("Panel.Fouls."+L.foulsLabel),s[side+"Fouls"],null,K.foulColor);
  T0.fouls2.box.hidden=!L.fouls2;
  if(L.fouls2)fillCounter(T0.fouls2,t("Panel.Fouls2."+L.fouls2Label),s[side+"Fouls2"],null,K.foulColor);
  var rows=L.penalties?(s.penalties[side]||[]):[];
  T0.penHead.hidden=!L.penalties;T0.penCap.textContent=t("Panel.Penalties");
  var key=CONTROL+JSON.stringify(rows.map(function(p){return [p.id,p.player,p.time,p.phase2,p.queued]}));
  if(key!==T0.penKey){T0.penKey=key;T0.penList.innerHTML="";
    rows.forEach(function(p){
      var row=el("div","pen"+(p.queued?" q":""));
      row.appendChild(el("span","",[p.player,p.queued?t("Panel.Queued"):"",p.time,p.phase2].filter(Boolean).join(" ")));
      if(CONTROL){var x=btn(ICON.cross,"ghost","PenaltyCancel",{id:p.id});x.title=t("Panel.PenaltyCancel");row.appendChild(x)}
      var m=el("i");m.style.width=(p.queued?100:p.percent)+"%";row.appendChild(m);T0.penList.appendChild(row)})}
  else rows.forEach(function(p,i){var m=T0.penList.children[i]&&T0.penList.children[i].lastChild;if(m)m.style.width=(p.queued?100:p.percent)+"%"})}

/* ---- the border of the clock card as the progress of the period ---- */
function paintBar(K){
  var card=$("card"),w=card.clientWidth,h=card.clientHeight,r=6,d=3;
  var svg=$("prog"),bar=$("bar");
  svg.setAttribute("viewBox","0 0 "+w+" "+h);
  var x0=d/2,y0=d/2,x1=w-d/2,y1=h-d/2;
  bar.setAttribute("d","M"+(w/2)+" "+y0+"H"+(x1-r)+"A"+r+" "+r+" 0 0 1 "+x1+" "+(y0+r)+"V"+(y1-r)+"A"+r+" "+r+" 0 0 1 "+(x1-r)+" "+y1+
    "H"+(x0+r)+"A"+r+" "+r+" 0 0 1 "+x0+" "+(y1-r)+"V"+(y0+r)+"A"+r+" "+r+" 0 0 1 "+(x0+r)+" "+y0+"H"+(w/2));
  bar.setAttribute("pathLength","1000");
  var done=K.periodBar==="off"||K.share<0?0:(K.periodBar==="remaining"?K.share:1-K.share);
  bar.setAttribute("stroke-dasharray",(done*1000)+" 1000");
  bar.setAttribute("stroke",K.periodBarColor||"#284cb8");
  bar.style.display=done>0?"":"none"}

/* the digits are as large as the space between the two button columns allows (the dock measures the same way) */
function fitClock(){
  var c=$("clock"),k=$("clk");
  if(!c.clientWidth)return;
  k.style.fontSize="100px";
  var w=k.offsetWidth||1,size=Math.max(24,Math.min(120,Math.floor(100*(c.clientWidth-4)/w)));
  k.style.fontSize=size+"px"}
function render(s){
  last=s;
  var K=s.look,L=s.layout;
  var r=document.documentElement.style;
  r.setProperty("--home",K.homeColor||"#284cb8");r.setProperty("--away",K.awayColor||"#ef7d57");
  $("sport").textContent=T["Sport."+s.sport]||s.sport;
  var c=$("clock");$("clk").textContent=s.clock;c.style.color=K.clockColor||"";fitClock();
  $("period").textContent=s.periodLabel;
  $("of").textContent=t("Panel.Of").replace("%1",s.regulationPeriods);
  $("stoppage").hidden=!L.stoppage;$("stopval").textContent=String(s.stoppageAnnouncedMinutes);
  var st=$("strength");st.hidden=!(L.strength&&s.strength);st.textContent=s.strength;
  if(CONTROL){
    var tg=$("toggle");tg.className=s.clockRunning?"stop":"go";
    var label=s.clockRunning?t("Dock.Stop"):t("Dock.Start");
    if(tg.dataset.l!==label+s.clockRunning){tg.dataset.l=label+s.clockRunning;tg.innerHTML=(s.clockRunning?ICON.pause:ICON.play)+label}
    $("reset").disabled=s.clockRunning;
    $("prev").disabled=$("next").disabled=s.clockRunning}
  paintTeam("home",s);paintTeam("away",s);paintBar(K)}

function badge(){var b=$("badge");
  b.className="badge"+(lost>2?" lost":(CONTROL?" live":""));
  b.textContent=lost>2?t("Web.Lost"):(CONTROL?t("Web.Control"):t("Web.ViewOnly"))}
function poll(){
  clearTimeout(timer);
  api("/api/state").then(function(r){lost=0;if(r.state)render(r.state);badge()})
   .catch(function(){lost++;badge()})
   .then(function(){timer=setTimeout(poll,350)})}

/* ---- start ---- */
var face=document.createElement("style");
face.textContent='@font-face{font-family:DSEG7;src:url("/font?k='+encodeURIComponent(KEY)+'") format("truetype")}';
document.head.appendChild(face);
Array.prototype.forEach.call(document.querySelectorAll(".ctl"),function(e){e.hidden=!CONTROL});
$("reset").innerHTML=ICON.reset+t("Dock.Reset");
$("prev").innerHTML=ICON.prev;$("next").innerHTML=ICON.next;
if(CONTROL)Array.prototype.forEach.call(document.querySelectorAll("[data-c]"),function(b){
  b.onclick=function(){send(b.dataset.c,b.dataset.a?JSON.parse(b.dataset.a):{})}});
if(CONTROL&&canVib){var v=$("vib");v.hidden=false;
  var paintVib=function(){v.textContent=t("Web.Vibration")+": "+(vibOn?t("Web.On"):t("Web.Off"))};
  paintVib();v.onclick=function(){vibOn=!vibOn;try{localStorage.setItem("sbVib",vibOn?"1":"0")}catch(e){}paintVib();buzz(20)}}
TEAM.home=makeTeam("home");TEAM.away=makeTeam("away");
window.addEventListener("resize",function(){fitClock();if(last.look)paintBar(last.look)});
if(document.fonts&&document.fonts.ready)document.fonts.ready.then(fitClock);
document.title="ScoreBoard";badge();poll();
</script></body></html>
)HTML";

} // namespace

std::string webPageHtml(bool canControl, const std::string& i18nJson) {
    std::string page = std::string(kStyle) + kScript;
    const auto replace = [&page](const std::string& token, const std::string& value) {
        const std::size_t at = page.find(token);
        if (at != std::string::npos) page.replace(at, token.size(), value);
    };
    replace("__CONTROL__", canControl ? "true" : "false");
    replace("__I18N__", i18nJson);
    return page;
}
