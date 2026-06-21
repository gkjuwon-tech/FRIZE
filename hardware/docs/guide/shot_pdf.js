// guide.html → FRIZE_사용가이드.pdf  (node shot_pdf.js, puppeteer 필요)
const puppeteer=require('puppeteer'), path=require('path');
(async()=>{ const b=await puppeteer.launch({args:['--no-sandbox','--disable-setuid-sandbox']});
  const p=await b.newPage();
  await p.goto('file://'+path.join(__dirname,'guide.html'),{waitUntil:'networkidle0',timeout:60000});
  await new Promise(r=>setTimeout(r,800));
  await p.pdf({path:path.join(__dirname,'..','FRIZE_사용가이드.pdf'),format:'A4',printBackground:true,
    margin:{top:'0',bottom:'0',left:'0',right:'0'}});
  await b.close(); console.log('pdf ok'); })().catch(e=>{console.error(e);process.exit(1)});
