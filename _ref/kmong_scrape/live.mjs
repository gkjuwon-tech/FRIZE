import { chromium } from 'playwright';
import fs from 'fs';
const SITES=[['rlwrld','https://www.rlwrld.ai/ko'],['headit','https://headit.me/'],['ukuya','https://ukuya-family.co.kr/']];
const b = await chromium.launch();
const ctx = await b.newContext({ ignoreHTTPSErrors:true, viewport:{width:1440,height:900}, userAgent:'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36' });
const p = await ctx.newPage();
for(const [name,url] of SITES){
  try{
    await p.goto(url,{waitUntil:'domcontentloaded',timeout:60000});
    await p.waitForTimeout(3500);
    let prev=-1;
    for(let i=0;i<120;i++){await p.evaluate(()=>window.scrollBy(0,700));await p.waitForTimeout(160);const y=await p.evaluate(()=>Math.round(scrollY));if(y===prev){await p.waitForTimeout(500);const y2=await p.evaluate(()=>Math.round(scrollY));if(y2===prev)break;}prev=y;}
    await p.waitForTimeout(1000);await p.evaluate(()=>scrollTo(0,0));await p.waitForTimeout(800);
    const f=`shots/live_${name}.png`;
    await p.screenshot({path:f,fullPage:true});
    console.log(`✓ ${name} | h=${await p.evaluate(()=>document.body.scrollHeight)} | ${Math.round(fs.statSync(f).size/1024)}KB`);
  }catch(e){console.log(`✗ ${name}: ${e.message.split('\n')[0]}`);}
}
await b.close();
