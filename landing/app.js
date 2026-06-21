// FRIZE site — minimal interactions
(function(){
  // header condense
  var nav=document.querySelector('header.nav');
  var onScroll=function(){ if(nav) nav.classList.toggle('scrolled', window.scrollY>20); };
  window.addEventListener('scroll',onScroll,{passive:true}); onScroll();

  // reveal on scroll
  var io=new IntersectionObserver(function(es){
    es.forEach(function(e){ if(e.isIntersecting){ e.target.classList.add('in'); io.unobserve(e.target); } });
  },{threshold:.12, rootMargin:'0px 0px -8% 0px'});
  document.querySelectorAll('.rv').forEach(function(el,i){ el.style.transitionDelay=(Math.min(i%5,4)*60)+'ms'; io.observe(el); });

  // mobile nav
  var burger=document.querySelector('.burger'), links=document.querySelector('.nav-links');
  if(burger&&links){ burger.addEventListener('click',function(){
    var open=links.style.display==='flex';
    links.style.cssText=open?'':'display:flex;position:absolute;top:64px;left:0;right:0;flex-direction:column;background:#0b0e13;border-bottom:1px solid rgba(255,255,255,.08);padding:16px var(--pad);gap:14px';
  });}

  // live clock (UTC) in any [data-clock]
  function tick(){ var t=new Date().toISOString().slice(11,19); document.querySelectorAll('[data-clock]').forEach(function(e){e.textContent=t+'Z';}); }
  setInterval(tick,1000); tick();

  // forms — demo only (no backend)
  document.querySelectorAll('form[data-demo]').forEach(function(f){
    f.addEventListener('submit',function(ev){ ev.preventDefault();
      var btn=f.querySelector('button[type=submit]'); if(btn){btn.textContent='접수됨 · RECEIVED';btn.disabled=true;}
      var note=f.querySelector('.note'); if(note) note.textContent='요청이 큐에 등록되었습니다. 영업일 기준 2일 내 회신 — REF '+Math.random().toString(36).slice(2,8).toUpperCase();
    });
  });
})();
