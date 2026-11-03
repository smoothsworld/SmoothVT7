const {chromium}=require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:1400,height:760},deviceScaleFactor:2});
await p.goto('file://'+process.argv[2]+'#bg');await p.waitForTimeout(400);
await p.locator('#plate').screenshot({path:process.argv[3]});await b.close();})();
