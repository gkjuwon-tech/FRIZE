import { chromium } from 'playwright';
import fs from 'fs';
const PICK = [
 [273983,'소띠크루 랜딩페이지'],[274013,'글로리아 웨딩홀 랜딩'],[274015,'LG유플러스샵 랜딩'],
 [274019,'리소프카페 랜딩'],[274371,'STF 랜딩'],[260864,'우쿠야 일식 프랜차이즈 랜딩'],
 [260890,'단가로 AI 서비스 랜딩'],[248809,'더금융파트너스 랜딩'],[239120,'더마클리닉 피부과 랜딩'],
 [224674,'AI 헬스케어 웨어러블 브랜드'],[224760,'Web3 플랫폼 UIUX'],[271223,'리얼월드 피지컬AI 글로벌'],
 [267564,'HL홀딩스 Fleet-ON'],[172666,'HEADIT 인터렉션 IT'],[205047,'OBO 디자인 에이전시'],
 [276548,'object-log 스크롤리텔링 매거진'],[230386,'강서 병원 비주얼중심'],[226769,'리네아 피부과 트렌디'],
 [52289,'프리미엄 가죽가방 쇼피파이'],[83897,'프리미엄 쥬얼리 브랜드'],[110661,'GRASSE 풀빌라 리조트'],
 [262496,'LG유플러스샵 브랜드웹'],[267200,'낙소당 반응형'],[224067,'벨루나로봇 AI'],[270256,'온라인강의 커머스'],
];
const b = await chromium.launch();
const ctx = await b.newContext({ ignoreHTTPSErrors:true, viewport:{width:1440,height:1000}, deviceScaleFactor:1 });
const p = await ctx.newPage();
const results=[];
for(const [id,label] of PICK){
  const url=`https://kmong.com/portfolio/view/${id}`;
  try{
    await p.goto(url,{waitUntil:'domcontentloaded',timeout:60000});
    await p.waitForTimeout(2500);
    // slow scroll to bottom to trigger lazy images & interactions
    let prev=-1;
    for(let i=0;i<80;i++){
      await p.evaluate(()=>window.scrollBy(0,900));
      await p.waitForTimeout(180);
      const y=await p.evaluate(()=>Math.round(window.scrollY));
      if(y===prev){await p.waitForTimeout(400);const y2=await p.evaluate(()=>Math.round(window.scrollY));if(y2===prev)break;}
      prev=y;
    }
    await p.waitForTimeout(800);
    await p.evaluate(()=>window.scrollTo(0,0));
    await p.waitForTimeout(500);
    const meta=await p.evaluate(()=>{
      const grab=lbl=>{const el=[...document.querySelectorAll('h1,h2,h3,h4,dt,th,strong,span')].find(e=>e.innerText.trim()===lbl);return el? (el.nextElementSibling?.innerText||el.parentElement?.innerText||'').replace(lbl,'').trim().slice(0,60):''};
      const txt=document.body.innerText;
      const ext=[...new Set([...document.querySelectorAll('a[href^="http"]')].map(a=>a.href).filter(h=>!/kmong|kakao|facebook|instagram|youtube|naver|google|apple|itunes|play\.google|ftc\.go|channel\.io|greetinghr|kmongcorp/.test(h)))];
      const textUrls=[...new Set((txt.match(/https?:\/\/[^\s)"']+/g)||[]).filter(h=>!/kmong/.test(h)))];
      return {title:document.title.replace(' - 크몽',''),client:grab('클라이언트'),industry:grab('업종'),style:grab('스타일'),period:grab('참여 기간'),scrollH:document.body.scrollHeight,ext:[...new Set([...ext,...textUrls])].slice(0,6)};
    });
    const file=`shots/pf_${id}.png`;
    await p.screenshot({path:file,fullPage:true});
    const kb=Math.round(fs.statSync(file).size/1024);
    results.push({id,label,url,file,...meta,sizeKB:kb});
    console.log(`✓ ${id} ${label} | h=${meta.scrollH} | ${kb}KB | ext=${meta.ext.length?meta.ext.join(','):'-'}`);
  }catch(e){console.log(`✗ ${id} ${label}: ${e.message.split('\n')[0]}`);results.push({id,label,url,error:e.message.split('\n')[0]});}
}
fs.writeFileSync('results.json',JSON.stringify(results,null,1));
console.log('\nDONE. captured',results.filter(r=>r.file).length,'/',PICK.length);
await b.close();
