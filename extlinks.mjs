import { chromium } from 'playwright';
const b = await chromium.launch();
const ctx = await b.newContext({ ignoreHTTPSErrors:true, viewport:{width:1440,height:1000} });
const p = await ctx.newPage();
await p.goto('https://kmong.com/gig/771070', { waitUntil:'domcontentloaded', timeout:60000 });
await p.waitForTimeout(2500);
// scroll to load
for(let i=0;i<15;i++){await p.evaluate(()=>window.scrollBy(0,1200));await p.waitForTimeout(200);}
// find portfolio links + all external links
const data = await p.evaluate(()=>{
  const all=[...document.querySelectorAll('a[href]')].map(a=>a.href);
  const ext=[...new Set(all.filter(h=>/^https?:/.test(h) && !/kmong\.com|kmong\.co|kakao|facebook|instagram|youtube|naver|google|apple|appstore|play\.google/.test(h)))];
  // portfolio heading?
  const hasPortfolio=[...document.querySelectorAll('h1,h2,h3,a,button,span')].some(e=>/포트폴리오/.test(e.innerText||''));
  return {ext, hasPortfolio, expertLink:(all.find(h=>/\/@|\/user|\/experts?\//.test(h))||'')};
});
console.log('hasPortfolio:', data.hasPortfolio);
console.log('expertLink:', data.expertLink);
console.log('EXTERNAL LINKS:', data.ext.length);
data.ext.forEach(l=>console.log('  ',l));
await b.close();
