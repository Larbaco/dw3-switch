(()=>{
'use strict';
const $=(s,r=document)=>r.querySelector(s),$$=(s,r=document)=>[...r.querySelectorAll(s)],$i=id=>document.getElementById(id);
const root=document.documentElement,body=document.body,main=$i('main');
root.dataset.enhanced='true';
const store=(k,v)=>{try{if(v===undefined)return localStorage.getItem(k);localStorage.setItem(k,v)}catch(e){return null}};
const esc=s=>s.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fold=s=>s.normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLocaleLowerCase('pt-BR');
const fragment=hash=>{try{return decodeURIComponent(hash.replace(/^#/,''))}catch{return ''}};
function focusTarget(el){if(!el)return;el.tabIndex=-1;el.focus({preventScroll:true})}
const mobile=()=>matchMedia('(max-width:900px)').matches;
// theme: saved choice > dark (default)
root.dataset.theme=store('dw3-theme')||'dark';
$i('theme').onclick=()=>{const t=root.dataset.theme==='light'?'dark':'light';root.dataset.theme=t;store('dw3-theme',t)};
// font size
let fs=+store('dw3-fs')||16;
const setFs=v=>{fs=Math.max(12,Math.min(24,v));root.style.setProperty('--fs',fs+'px');store('dw3-fs',fs)};
setFs(fs);$i('fsm').onclick=()=>setFs(fs-1);$i('fsp').onclick=()=>setFs(fs+1);
// mobile menu
const menu=$i('menu');
const setOpen=o=>{body.classList.toggle('open',o);menu.setAttribute('aria-expanded',String(o));
 $i('dmenu').setAttribute('aria-expanded',String(o));$i('side').inert=mobile()&&!o};
setOpen(false);
addEventListener('resize',()=>{setOpen(body.classList.contains('open'));frame()});
menu.onclick=()=>setOpen(!body.classList.contains('open'));
$i('dmenu').onclick=()=>setOpen(!body.classList.contains('open'));
$i('side').addEventListener('click',e=>{if(e.target.closest('a'))setOpen(false)});
main.addEventListener('click',()=>{if(body.classList.contains('open'))setOpen(false)});
// sidebar groups remembered
const gstate=(()=>{try{return JSON.parse(store('dw3-groups')||'{}')}catch(e){return {}}})();
const groups=$$('#tree details.grp');
groups.forEach(d=>{const k=d.dataset.sec;if(k in gstate)d.open=!!gstate[k];
 d.addEventListener('toggle',()=>{gstate[k]=d.open;store('dw3-groups',JSON.stringify(gstate))})});
// scroll-spy, breadcrumb, progress, dock
const links=new Map();$$('#tree a[href^="#"]').forEach(a=>links.set(a.getAttribute('href').slice(1),a));
const heads=$$('section.lvl1,article.lvl2');
const navs=$$('article.lvl2,section.lvl1[data-leaf]');
const crumb=$i('crumb'),pbar=$i('prog'),topb=$i('top'),dprev=$i('dprev'),dnext=$i('dnext');
let cur=undefined,saveTimer;
function setActive(id){
 if(cur===id)return;cur=id;
 $$('#tree a.active').forEach(a=>{a.classList.remove('active');a.removeAttribute('aria-current')});
 if(!id){crumb.innerHTML='<span>Início</span>'}
 else{
  const el=$i(id),isCh=el.classList.contains('lvl2'),sec=isCh?$i(el.dataset.parent):el;
  crumb.innerHTML=`<span>${esc(sec.dataset.title)}</span>`+(isCh?`<i>›</i><b>${esc(el.dataset.title)}</b>`:'');
 }
 const a=id&&links.get(id);
 if(a){
  a.classList.add('active');a.setAttribute('aria-current','location');
  const d=a.closest('details');if(d)d.open=true;
  const r=a.getBoundingClientRect(),s=$i('side').getBoundingClientRect();
  if($i('results').hidden&&(r.top<s.top+40||r.bottom>s.bottom-40)){
   const tree=$i('tree');tree.scrollTop+=r.top-tree.getBoundingClientRect().top-80;
  }
 }
 groups.forEach(d=>{const ls=$$('li a',d);const i=ls.findIndex(x=>x.classList.contains('active'));d.style.setProperty('--p',i<0?0:(i+1)/ls.length)});
 const ix=navs.findIndex(e=>e.id===id);
 const pv=navs[ix-1],nx=ix<0?navs[0]:navs[ix+1];
 dprev.href=pv?'#'+pv.id:'#inicio';dprev.setAttribute('aria-disabled',String(!pv));
 dnext.href=nx?'#'+nx.id:'#inicio';dnext.setAttribute('aria-disabled',String(!nx));
 const chapter=id&&$i(id),show=chapter&&chapter.matches('article.lvl2');
 $i('context').hidden=!show;
 $i('area-nav').replaceChildren();
 clearTimeout(saveTimer);
 if(show){
  $$('.guide-area>.subheading',chapter).forEach((h,i)=>{
   const a=document.createElement('a');a.href='#'+h.id;a.textContent=(i+1)+'. '+h.textContent;
   $i('area-nav').append(a);
  });
  const saved={id:chapter.id,title:chapter.dataset.title};
  saveTimer=setTimeout(()=>store('dw3-reading',JSON.stringify(saved)),500);
 }
}
function frame(){
 tick=false;
 let id=null;
 const edge=parseFloat(getComputedStyle(root).getPropertyValue('--bh'))+30;
 if(scrollY>200)for(const h of heads){if(h.getBoundingClientRect().top<=edge)id=h.id;else break}
 setActive(id);
 const max=document.documentElement.scrollHeight-innerHeight;
 pbar.style.width=(max>0?Math.min(100,scrollY/max*100):0)+'%';
 topb.classList.toggle('show',scrollY>800);
 if(id&&$i(id).matches('article.lvl2')){
  const ch=$i(id),r=ch.getBoundingClientRect(),progress=Math.round(Math.min(100,Math.max(0,(edge-r.top)/Math.max(1,r.height-innerHeight+edge)*100)));
  $i('chapter-progress').value=progress;$i('reading-status').textContent=progress+'% deste capítulo · progresso de leitura';
  let active=null;for(const h of $$('.guide-area>.subheading',ch)){if(h.getBoundingClientRect().top<=edge+8)active=h.id;else break}
  $$('#area-nav a').forEach(a=>{if(fragment(a.hash)===active)a.setAttribute('aria-current','location');else a.removeAttribute('aria-current')});
 }
}
let tick=false;
addEventListener('scroll',()=>{if(!tick){tick=true;setTimeout(frame,60)}},{passive:true});
topb.onclick=()=>{scrollTo({top:0});focusTarget($i('inicio'))};
// keyboard: j/k move between chapters
function go(dir){
 const i=navs.findIndex(e=>e.id===cur);
 const el=i<0?navs[dir>0?0:navs.length-1]:navs[i+dir];
 if(el)navigate(el.id);
}
// search
const SKIP='.chb,.pn,ul.idx,h3.ch,h2,.tiles,details.orig,.nosearch,.entity-tools';
const txt=n=>n.nodeType===3?n.data:(n.nodeType!==1||n.matches(SKIP)?'':[...n.childNodes].map(txt).join(''));
const secs=$$('article.lvl2,section.lvl1').map(el=>{
 const lvl1=el.classList.contains('lvl1');
 const own=[...el.children].filter(n=>!n.matches(SKIP+(lvl1?',article':'')));
 return {el,id:el.id,title:el.dataset.title,parent:lvl1?null:el.dataset.parent,text:own.map(txt).join('\n')};
}).filter(s=>s.text.trim());
const input=$i('q'),res=$i('results'),tree=$i('tree');
const scope=$i('search-scope');
scope.addEventListener('change',run);
function resetSearch(){input.value='';scope.value='';run()}
$i('search-clear').onclick=()=>{resetSearch();input.focus()};
let timer;
$i('sform').addEventListener('submit',e=>e.preventDefault());
input.addEventListener('input',()=>{clearTimeout(timer);timer=setTimeout(run,150)});
function run(){
 clearMarks();
 const q=input.value.trim();
 body.classList.toggle('searching',q.length>=2);
 $i('side').querySelector('.search-tools').hidden=q.length<2;
 if(q.length<2){res.hidden=true;tree.hidden=false;$i('search-status').textContent='';return}
 if(mobile())setOpen(true);
 const lq=fold(q);let total=0,html='';
 for(const s of secs){
  if(scope.value&&s.id!==scope.value&&s.parent!==scope.value)continue;
  const text=s.title+'\n'+s.text,lt=fold(text);let i=lt.indexOf(lq),n=0,first=-1;
  while(i>=0){n++;if(first<0)first=i;i=lt.indexOf(lq,i+lq.length)}
  if(!n)continue;total+=n;
  const a=Math.max(0,first-50),b=Math.min(text.length,first+lq.length+70);
  const snip=(a?'…':'')+text.slice(a,b).replace(/\s+/g,' ')+(b<text.length?'…':'');
  html+=`<a class="r" href="#${s.id}" data-q="${esc(q)}"><b>${esc(s.title)} <span class="n">(${n})</span></b><small>${esc(snip)}</small></a>`;
 }
 $i('search-status').textContent=total+' ocorrência(s) para '+q;
 res.innerHTML=`<div class="cnt">${total} ocorrência(s) para “${esc(q)}”</div>`+html+(total?'':'<p class="cnt">Tente outro nome ou busque em todo o guia. O original em inglês recolhido não entra na busca.</p>');
 res.hidden=false;tree.hidden=true;
}
res.addEventListener('click',e=>{const a=e.target.closest('a.r');if(!a)return;
 setTimeout(()=>highlight(a.getAttribute('href').slice(1),a.dataset.q),0)});
function clearMarks(){$$('mark').forEach(m=>{const p=m.parentNode;p.replaceChild(document.createTextNode(m.textContent),m);p.normalize()})}
function highlight(id,q){
 clearMarks();const el=$i(id);if(!el)return;
 const lq=fold(q);const w=document.createTreeWalker(el,NodeFilter.SHOW_TEXT);const nodes=[];
 while(w.nextNode())nodes.push(w.currentNode);
 let first=null;
 for(const t of nodes){
  if(t.parentNode.closest('.chb,.pn,ul.idx,h3.ch,details.orig,button,script,style'))continue;
  const lt=fold(t.data);let i=lt.indexOf(lq);if(i<0)continue;
  const frag=document.createDocumentFragment();let last=0;
  while(i>=0){frag.append(t.data.slice(last,i));const m=document.createElement('mark');
   m.textContent=t.data.slice(i,i+lq.length);frag.append(m);if(!first)first=m;
   last=i+lq.length;i=lt.indexOf(lq,last)}
  frag.append(t.data.slice(last));t.replaceWith(frag);
 }
 if(first){reveal(first);first.scrollIntoView({block:'center'});focusTarget(first)}
}
// command palette
const PAL=JSON.parse($i('pal-data').textContent);
const pal=$i('pal'),pq=$i('pq'),pl=$i('pl');
const KLAB={sec:'Seção',cap:'Capítulo',loc:'Local',ent:'Entidade'};
let psel=0,plist=[];
function fuzzy(t,q){const i=t.indexOf(q);if(i>=0)return i;let p=0,last=-1,gap=0;
 for(const ch of q){const j=t.indexOf(ch,p);if(j<0)return -1;if(last>=0)gap+=j-last-1;last=j;p=j+1}
 return 1000+gap}
function palRender(){
 const q=fold(pq.value.trim());
 plist=!q?PAL.filter(x=>x.k==='sec'||x.k==='cap').slice(0,40)
  :PAL.map(x=>[fuzzy(fold(x.t+' · '+x.g+' '+(x.en||'')),q),x]).filter(a=>a[0]>=0).sort((a,b)=>a[0]-b[0]).slice(0,60).map(a=>a[1]);
 psel=Math.max(0,Math.min(psel,plist.length-1));
 pl.innerHTML=plist.map((x,i)=>`<li role="option" id="po${i}" aria-selected="${i===psel}" class="${i===psel?'on':''}" data-i="${i}"><span class="pk">${KLAB[x.k]}</span><span class="pt">${esc(x.t)}</span><span class="pg">${esc(x.g)}</span></li>`).join('')
  ||'<li class="none">Nada encontrado</li>';
 const on=$i('po'+psel);if(on){on.scrollIntoView({block:'nearest'});pq.setAttribute('aria-activedescendant',on.id)}else pq.removeAttribute('aria-activedescendant');
}
let returnFocus=null;
function modalState(open){main.inert=open;$i('bar').inert=open;$i('side').inert=open||(mobile()&&!body.classList.contains('open'));$i('dock').inert=open;$i('top').inert=open}
function openPal(){if(!pal.hidden)return;if(!kbdm.hidden)closeKeys();returnFocus=document.activeElement;pal.hidden=false;modalState(true);pq.value='';psel=0;palRender();pq.focus()}
function closePal(){pal.hidden=true;modalState(false);if(returnFocus)returnFocus.focus()}
function jump(x){closePal();navigate(x.id)}
$i('pbtn').onclick=openPal;
$('.modal-close',pal).onclick=closePal;
pq.addEventListener('input',()=>{psel=0;palRender()});
pq.addEventListener('keydown',e=>{
 if(e.key==='ArrowDown'){e.preventDefault();psel=Math.min(plist.length-1,psel+1);palRender()}
 else if(e.key==='ArrowUp'){e.preventDefault();psel=Math.max(0,psel-1);palRender()}
 else if(e.key==='Enter'){e.preventDefault();if(plist[psel])jump(plist[psel])}
});
pl.addEventListener('click',e=>{const li=e.target.closest('li[data-i]');if(li)jump(plist[+li.dataset.i])});
pal.addEventListener('click',e=>{if(e.target===pal)closePal()});
// shortcuts overlay
const kbdm=$i('kbd');
function closeKeys(){kbdm.hidden=true;modalState(false);if(returnFocus)returnFocus.focus()}
kbdm.addEventListener('click',e=>{if(e.target===kbdm)closeKeys()});
$('.modal-close',kbdm).onclick=closeKeys;
// global keys
addEventListener('keydown',e=>{
 if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='k'){e.preventDefault();openPal();return}
 if(e.key==='Tab'&&(!pal.hidden||!kbdm.hidden)){
  const targets=$$('input,button,a[href]',!pal.hidden?pal:kbdm),i=targets.indexOf(document.activeElement);
  e.preventDefault();targets[(i+(e.shiftKey?-1:1)+targets.length)%targets.length].focus();return;
 }
 const typing=/^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement.tagName);
 if(e.key==='Escape'){
  if(!pal.hidden){closePal();return}
  if(!kbdm.hidden){closeKeys();return}
  if(body.classList.contains('open')&&mobile()){setOpen(false);return}
  if(document.activeElement===input){resetSearch();input.blur()}
  return;
 }
 if(typing||!pal.hidden||!kbdm.hidden||e.altKey||e.ctrlKey||e.metaKey)return;
 if(e.key==='/'){e.preventDefault();input.focus();input.select()}
 else if(e.key==='?'){e.preventDefault();returnFocus=document.activeElement;kbdm.hidden=false;modalState(true);const box=kbdm.querySelector('.mbox');box.tabIndex=-1;box.focus()}
 else if(e.key==='j'){go(1)}
 else if(e.key==='k'){go(-1)}
});
$i('dsearch').onclick=()=>{input.focus();input.select()};
// digivolution tree: expand / collapse all
document.addEventListener('click',e=>{const b=e.target.closest('.dtools button');if(!b)return;
 const open=b.dataset.act==='open';b.closest('.dtwrap').querySelectorAll('details').forEach(d=>d.open=open)});
// strategy notes start collapsed on small screens
if(mobile())$$('details.strat').forEach(d=>d.open=false);
// sortable tables (numeric aware), live filters, section fade-in
function sortTable(th){
 const table=th.closest('table'),idx=[...th.parentNode.children].indexOf(th);
 const asc=th.getAttribute('aria-sort')!=='ascending';
 table.querySelectorAll('thead th').forEach(h=>h.removeAttribute('aria-sort'));
 th.setAttribute('aria-sort',asc?'ascending':'descending');
 const tb=table.tBodies[0],rows=[...tb.rows];
 const key=r=>{const c=r.cells[idx];if(!c)return '';const t=(c.dataset.v!==undefined?c.dataset.v:c.textContent).trim();
  const n=parseFloat(t.replace(/,/g,''));return /^[-+]?\d/.test(t)&&!isNaN(n)?n:t.toLowerCase()};
 rows.sort((a,b)=>{const x=key(a),y=key(b);
  if(typeof x==='number'&&typeof y==='number')return asc?x-y:y-x;
  return asc?String(x).localeCompare(String(y)):String(y).localeCompare(String(x))});
 rows.forEach(r=>tb.appendChild(r));
}
$$('table.grid thead th').forEach(th=>{th.tabIndex=0;th.title='Clique para ordenar'});
document.addEventListener('click',e=>{const th=e.target.closest('table.grid thead th');if(th)sortTable(th)});
document.addEventListener('keydown',e=>{if(e.key!=='Enter')return;const th=e.target.closest&&e.target.closest('table.grid thead th');if(th)sortTable(th)});
document.addEventListener('input',e=>{const inp=e.target.closest('input.tfilt');if(!inp)return;
 const q=fold(inp.value.trim()),root=inp.closest('.fbox');
 const rows=$$('tbody tr, ul.cgrid li',root);rows.forEach(el=>{el.hidden=!!q&&!fold(el.textContent).includes(q)});
 let count=$('.filter-count',root);if(!count){count=document.createElement('span');count.className='filter-count';count.setAttribute('role','status');inp.after(count)}
 count.textContent=rows.filter(el=>!el.hidden).length+' de '+rows.length+' entradas';
});
// Reference filters never remove destinations. A deep link restores the target.
const entityQuery=$i('entity-query'),entityKind=$i('entity-kind'),entries=$$('.entity-entry');
function filterEntities(){
 const q=fold(entityQuery.value.trim()),kind=entityKind.value;
 entries.forEach(el=>{el.hidden=!!(q&&!fold(el.textContent).includes(q)||kind&&!el.dataset.roles.split('|').includes(kind))});
 const rank=el=>{const name=fold($('h3',el).textContent);return !q?0:name===q?0:name.startsWith(q)?1:name.includes(q)?2:3};
 entries.slice().sort((a,b)=>rank(a)-rank(b)).forEach(el=>el.parentElement.append(el));
 const n=entries.filter(el=>!el.hidden).length;
 $i('entity-count').textContent=n+' de '+entries.length+' entidades'+(n?'':' · Nenhum resultado. Limpe os filtros ou tente outro nome.');
}
entityQuery.addEventListener('input',filterEntities);entityKind.addEventListener('change',filterEntities);
$i('entity-clear').onclick=()=>{entityQuery.value='';entityKind.value='';filterEntities();entityQuery.focus()};
function reveal(el){
 if(el.closest('.entity-entry[hidden]')){entityQuery.value='';entityKind.value='';filterEntities()}
 for(let parent=el;parent&&parent!==main;parent=parent.parentElement){
  if(parent.matches('details'))parent.open=true;
  if(parent.hidden)parent.hidden=false;
 }
 const box=el.closest('.fbox');if(box){const input=$('.tfilt',box);if(input&&input.value){input.value='';input.dispatchEvent(new Event('input',{bubbles:true}))}}
}
function navigate(id,push=true){
 const el=$i(id);if(!el)return;
 reveal(el);setOpen(false);if(push&&fragment(location.hash)!==id)history.pushState(null,'','#'+encodeURIComponent(id));
 el.scrollIntoView({block:'start',behavior:'instant'});focusTarget(el);frame();
}
document.addEventListener('click',e=>{
 const a=e.target.closest('a[href^="#"]');if(!a||e.ctrlKey||e.metaKey||e.shiftKey||e.altKey||e.button!==0)return;
 const id=fragment(a.hash);if(!$i(id))return;e.preventDefault();navigate(id);
});
addEventListener('hashchange',()=>navigate(fragment(location.hash),false));
try{const saved=JSON.parse(store('dw3-reading')||'null');if(saved&&$i(saved.id)){
 const resume=$i('resume');resume.href='#'+saved.id;resume.textContent='Continuar: '+saved.title;resume.hidden=false;
}}catch{}
// deep link
if(location.hash)setTimeout(()=>navigate(fragment(location.hash),false),0);
frame();
})();
