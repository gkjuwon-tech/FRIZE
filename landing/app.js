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

// ── 브랜드 배경: 포인트클라우드 플라이스루(라이다 트윈 모티프) ──
(function(){
  var c=document.createElement('canvas'); c.id='fxbg'; document.body.prepend(c);
  var ctx=c.getContext('2d'), W,H, DPR=Math.min(2,window.devicePixelRatio||1), run=true;
  var css=getComputedStyle(document.documentElement);
  var ACC=(css.getPropertyValue('--accent-rgb')||'55,192,242').trim();
  function resize(){ W=c.width=innerWidth*DPR; H=c.height=innerHeight*DPR; c.style.width=innerWidth+'px'; c.style.height=innerHeight+'px'; }
  resize(); window.addEventListener('resize',resize);
  document.addEventListener('visibilitychange',function(){ run=!document.hidden; if(run) frame(); });

  var N = (innerWidth<700)?90:210, P=[];
  function spawn(p){ p.x=(Math.random()*2-1); p.y=(Math.random()*2-1); p.z=Math.random()*0.9+0.1; p.s=Math.random(); }
  for(var i=0;i<N;i++){ var p={}; spawn(p); P.push(p); }
  var t=0;
  function frame(){
    if(!run) return;
    t+=1; ctx.clearRect(0,0,W,H);
    var cx=W*0.62, cy=H*0.42, f=Math.min(W,H)*0.95;
    // 미세 스캔 밴드(천천히 이동)
    var bandY=((t*0.6)%(H+300))-150;
    var g=ctx.createLinearGradient(0,bandY-160,0,bandY+160);
    g.addColorStop(0,'rgba('+ACC+',0)'); g.addColorStop(0.5,'rgba('+ACC+',0.07)'); g.addColorStop(1,'rgba('+ACC+',0)');
    ctx.fillStyle=g; ctx.fillRect(0,bandY-160,W,320);
    var prev=null;
    for(var i=0;i<P.length;i++){
      var p=P[i]; p.z-=0.0016; if(p.z<=0.04){ spawn(p); p.z=1; }
      var k=f*0.5/p.z, sx=cx+p.x*k, sy=cy+p.y*k;
      var depth=1-p.z, sz=Math.max(DPR, depth*3.0*DPR), a=depth*depth*0.78;
      if(sx<-20||sx>W+20||sy<-20||sy>H+20) continue;
      // 화점처럼 가끔 따뜻한 점 섞기(소수)
      var warm = p.s>0.9;
      ctx.fillStyle = warm ? 'rgba(220,120,80,'+(a*0.95)+')' : 'rgba('+ACC+','+a+')';
      ctx.fillRect(sx,sy,sz,sz);
      // 근접 점 연결(메쉬 느낌, 가까운 것만)
      if(prev && depth>0.55){ var dx=sx-prev.x, dy=sy-prev.y; if(dx*dx+dy*dy<13000*DPR*DPR){
        ctx.strokeStyle='rgba('+ACC+','+(a*0.18)+')'; ctx.lineWidth=DPR*0.6;
        ctx.beginPath(); ctx.moveTo(prev.x,prev.y); ctx.lineTo(sx,sy); ctx.stroke(); } }
      prev={x:sx,y:sy};
    }
    requestAnimationFrame(frame);
  }
  frame();
})();
