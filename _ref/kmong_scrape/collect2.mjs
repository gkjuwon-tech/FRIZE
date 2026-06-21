import { chromium } from 'playwright';
import fs from 'fs';
const b = await chromium.launch();
const ctx = await b.newContext({ ignoreHTTPSErrors:true, viewport:{width:1440,height:1200} });
const p = await ctx.newPage();
const kw=encodeURIComponent('홈페이지 제작');
const ids=new Map(); // id -> title
for(let page=1; page<=4; page++){
  const u=`https://kmong.com/search?type=portfolios&page=${page}&sort=RANKING_POINTS&service=web&keyword=${kw}`;
  await p.goto(u,{waitUntil:'networkidle',timeout:60000});
  await p.waitForTimeout(2500);
  for(let i=0;i<8;i++){await p.evaluate(()=>window.scrollBy(0,1500));await p.waitForTimeout(300);}
  const items=await p.evaluate(()=>{
    const m={};
    document.querySelectorAll('a[href*="portfolio_id="]').forEach(a=>{
      const id=(a.getAttribute('href').match(/portfolio_id=(\d+)/)||[])[1];
      if(!id)return;
      const img=a.querySelector('img');
      const t=(a.innerText||img?.alt||'').trim().replace(/\s+/g,' ').slice(0,90);
      if(!m[id]||t.length>m[id].length) m[id]=t;
    });
    return m;
  });
  Object.entries(items).forEach(([id,t])=>{if(!ids.has(id))ids.set(id,t)});
  console.log('page',page,'-> total',ids.size);
}
const list=[...ids].map(([id,t])=>({id,t}));
fs.writeFileSync('web_portfolios.json',JSON.stringify(list,null,1));
console.log('\nSAVED',list.length,'web portfolios');
list.forEach(o=>console.log(o.id,'|',o.t||'(no title)'));
await b.close();
