// SPDX-License-Identifier: GPL-2.0-or-later
#include "WebPage.hpp"

// The remote page: one self-contained document (no libraries, no external requests) that polls
// /api/state and, when the viewer holds the control link, posts the same commands the dock sends.
// The text comes from the plugin's locale files through the __I18N__ object, so the page speaks the
// language of OBS. Adjacent literals stay under the 16 KB limit of one MSVC string.

namespace {

constexpr const char* kHead = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="robots" content="noindex"><title>ScoreBoard</title>
<style>
:root{--bg:#1d1f26;--card:#272a33;--inset:#15171c;--bdr:#3c404d;--btn:#3c404d;--btn2:#4a4f5f;
--txt:#eceef3;--mut:#9399a9;--go:#2ea043;--stop:#c01c37;--home:#4b7bec;--away:#ef7d57;--gold:#f2c94c}
@media(prefers-color-scheme:light){:root{--bg:#eef0f4;--card:#fff;--inset:#e4e7ee;--bdr:#c9cedb;
--btn:#e1e5ee;--btn2:#d2d8e6;--txt:#1b1e27;--mut:#5d6477}}
*{box-sizing:border-box}html,body{margin:0}
body{background:var(--bg);color:var(--txt);font:16px/1.35 system-ui,-apple-system,"Segoe UI",sans-serif;
padding:12px max(12px,env(safe-area-inset-right)) 24px max(12px,env(safe-area-inset-left));
-webkit-text-size-adjust:100%}
main{max-width:640px;margin:0 auto;display:grid;gap:10px}
.bar{display:flex;justify-content:space-between;align-items:center;color:var(--mut);font-size:13px}
.badge{border:1px solid var(--bdr);border-radius:999px;padding:2px 10px}
.badge.live{color:var(--go);border-color:var(--go)}.badge.lost{color:var(--stop);border-color:var(--stop)}
.card{background:var(--card);border:1px solid var(--bdr);border-radius:10px;padding:12px}
.clock{background:var(--inset);text-align:center;padding:10px 8px}
#clock{font:700 clamp(54px,22vw,128px)/1 ui-monospace,"Cascadia Mono",Consolas,monospace;
font-variant-numeric:tabular-nums;letter-spacing:.02em}
#period{color:var(--mut);margin-top:4px}
.stoppage{color:var(--gold);font-weight:600}
button{font:inherit;font-weight:600;color:var(--txt);background:var(--btn);border:1px solid var(--bdr);
border-radius:8px;min-height:44px;padding:0 14px;touch-action:manipulation;cursor:pointer}
button:active{background:var(--btn2)}button:disabled{opacity:.45}
.row{display:grid;gap:8px;grid-auto-flow:column;grid-auto-columns:1fr}
.go{background:var(--go);border-color:var(--go);color:#fff}.stop{background:var(--stop);border-color:var(--stop);color:#fff}
.teams{display:grid;gap:10px;grid-template-columns:1fr 1fr}
.team{border-top:3px solid var(--home);padding:10px}.team.away{border-top-color:var(--away)}
.name{text-align:center;font-weight:700;font-size:18px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.cap{color:var(--mut);font-size:11px;font-weight:600;text-transform:uppercase;letter-spacing:.06em;
text-align:center;margin:10px 0 4px}
.val{font:700 40px/1.1 system-ui,sans-serif;text-align:center;background:var(--gold);color:#111;
border-radius:8px;padding:6px 0;font-variant-numeric:tabular-nums}
.val.small{background:var(--inset);color:var(--txt);font-size:24px;padding:4px 0}
.seg{display:grid;grid-template-columns:48px 1fr 48px;gap:6px;align-items:stretch}
.seg.big{grid-template-columns:48px 1fr repeat(var(--n,1),48px)}
.pen{display:flex;align-items:center;justify-content:space-between;gap:6px;background:var(--inset);
border:1px solid var(--bdr);border-radius:8px;padding:4px 4px 4px 10px;margin-top:6px;font-weight:600}
.pen.q{color:var(--mut);font-weight:400}.pen button{min-height:32px;padding:0 10px}
.pill{justify-self:center;border:1px solid var(--bdr);border-radius:999px;padding:2px 12px;background:var(--card)}
.msg{text-align:center;color:var(--stop);min-height:1.4em;font-size:14px}
[hidden]{display:none!important}
</style></head><body><main>
<div class="bar"><span id="sport"></span><span id="badge" class="badge"></span></div>
<section class="card clock"><div id="clock">--:--</div><div id="period"></div><div id="stop" class="stoppage"></div></section>
<div id="ctl" hidden>
 <div class="row"><button data-c="ClockAdjust" data-a='{"deltaTenths":600}'>+1:00</button>
 <button data-c="ClockAdjust" data-a='{"deltaTenths":-600}'>-1:00</button>
 <button data-c="ClockAdjust" data-a='{"deltaTenths":10}'>+0:01</button>
 <button data-c="ClockAdjust" data-a='{"deltaTenths":-10}'>-0:01</button></div>
 <div class="row" style="margin-top:8px;grid-template-columns:2fr 1fr"><button id="toggle" data-c="ClockToggle"></button>
 <button id="reset" data-c="ClockReset"></button></div>
 <div class="row" style="margin-top:8px"><button id="prev" data-c="PeriodPrev"></button><button id="next" data-c="PeriodNext"></button></div>
</div>
<div class="teams"><section class="card team" id="home"></section><section class="card team away" id="away"></section></div>
<div id="strength" class="pill" hidden></div>
<div id="msg" class="msg"></div>
</main>
)HTML";

constexpr const char* kScript = R"HTML(<script>
"use strict";
var CONTROL=__CONTROL__, T=__I18N__, KEY=new URLSearchParams(location.search).get("k")||"";
var $=function(id){return document.getElementById(id)}, t=function(k){return T[k]||k};
var last="", lost=0, msgTimer=0;
function say(text){$("msg").textContent=text||"";clearTimeout(msgTimer);if(text)msgTimer=setTimeout(function(){say("")},4000)}
function api(path,body){
  return fetch(path+"?k="+encodeURIComponent(KEY),body===undefined?{cache:"no-store"}:
    {method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(body)}).then(function(r){return r.json()})}
function send(name,args){
  return api("/api/"+name,args||{}).then(function(r){
    if(r.result==="confirmation"){
      if(confirm(t("Reason."+r.reason))){args=Object.assign({},args,{confirmed:true});return send(name,args)}
    }else if(r.result==="rejected")say(t("Reason."+r.reason));
    else if(r.result==="error")say(r.error);
    poll(true)}).catch(function(){say(t("Web.Lost"))})}
function el(tag,cls,text){var e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e}
function btn(text,name,args){var b=el("button","",text);b.onclick=function(){send(name,args)};return b}
function counter(team,stat,caption,value,plus){
  var f=document.createDocumentFragment();
  f.appendChild(el("div","cap",caption));
  var s=el("div","seg"+(plus&&plus.length>1?" big":""));
  if(plus)s.style.setProperty("--n",plus.length);
  var v=el("div","val"+(stat==="score"?"":" small"),String(value));
  if(CONTROL){
    s.appendChild(btn("−","AddStat",{team:team,stat:stat,delta:-1}));s.appendChild(v);
    (plus||[1]).forEach(function(n){s.appendChild(btn("+"+n,"AddStat",{team:team,stat:stat,delta:n}))});
  }else{s.style.gridTemplateColumns="1fr";s.appendChild(v)}
  f.appendChild(s);return f}
function paint(side,s){
  var box=$(side),L=s.layout,team=side,e=box.firstChild;
  var h=document.createDocumentFragment();
  h.appendChild(el("div","name",s[side+"Name"]));
  h.appendChild(counter(team,"score",t("Panel.Score."+L.scoreLabel),s[side+"Score"],L.scoreValues));
  if(L.shots)h.appendChild(counter(team,"shots",t("Panel.Shots"),s[side+"Shots"]));
  if(L.fouls)h.appendChild(counter(team,"fouls",t("Panel.Fouls."+L.foulsLabel),s[side+"Fouls"]));
  if(L.fouls2)h.appendChild(counter(team,"fouls2",t("Panel.Fouls2."+L.fouls2Label),s[side+"Fouls2"]));
  if(L.penalties&&(s.penalties[side]||[]).length){
    h.appendChild(el("div","cap",t("Panel.Penalties")));
    s.penalties[side].forEach(function(p){
      var row=el("div","pen"+(p.queued?" q":""));
      row.appendChild(el("span","",[p.player,p.queued?t("Panel.Queued"):"",p.time,p.phase2].filter(Boolean).join(" ")));
      if(CONTROL){var x=btn("✕","PenaltyCancel",{id:p.id});x.title=t("Panel.PenaltyCancel");row.appendChild(x)}
      h.appendChild(row)})}
  box.replaceChildren(h)}
function render(s){
  $("sport").textContent=T["Sport."+s.sport]||s.sport;
  $("clock").textContent=s.clock;
  $("period").textContent=s.periodLabel+(s.regulationPeriods>1?" · "+t("Panel.Of").replace("%1",s.regulationPeriods):"");
  $("stop").textContent=s.layout.stoppage&&s.stoppageAnnounced?s.stoppageAnnounced:"";
  var st=$("strength");st.hidden=!(s.layout.strength&&s.strength);st.textContent=s.strength;
  if(CONTROL){
    var tg=$("toggle");tg.textContent=s.clockRunning?t("Dock.Stop"):t("Dock.Start");
    tg.className=s.clockRunning?"stop":"go";
    $("reset").textContent=t("Dock.Reset");$("reset").disabled=s.clockRunning;
    $("prev").textContent="‹ "+t("Panel.Segment."+s.layout.segmentLabel);
    $("next").textContent=t("Panel.Segment."+s.layout.segmentLabel)+" ›";
    $("prev").disabled=$("next").disabled=s.clockRunning;
  }
  var sig=JSON.stringify(s);
  if(sig!==last){last=sig;paint("home",s);paint("away",s)}}
function badge(){var b=$("badge");
  b.className="badge"+(lost>2?" lost":(CONTROL?" live":""));
  b.textContent=lost>2?t("Web.Lost"):(CONTROL?t("Web.Control"):t("Web.ViewOnly"))}
var timer=0;
function poll(now){
  clearTimeout(timer);
  api("/api/state").then(function(r){lost=0;if(r.state)render(r.state);badge()})
   .catch(function(){lost++;badge()})
   .then(function(){timer=setTimeout(poll,350)})}
$("ctl").hidden=!CONTROL;
if(CONTROL)Array.prototype.forEach.call(document.querySelectorAll("[data-c]"),function(b){
  b.onclick=function(){send(b.dataset.c,b.dataset.a?JSON.parse(b.dataset.a):{})}});
document.title="ScoreBoard";badge();poll();
</script></body></html>
)HTML";

} // namespace

std::string webPageHtml(bool canControl, const std::string& i18nJson) {
    std::string page = std::string(kHead) + kScript;
    const auto replace = [&page](const std::string& token, const std::string& value) {
        const std::size_t at = page.find(token);
        if (at != std::string::npos) page.replace(at, token.size(), value);
    };
    replace("__CONTROL__", canControl ? "true" : "false");
    replace("__I18N__", i18nJson);
    return page;
}
