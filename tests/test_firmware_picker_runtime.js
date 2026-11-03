#!/usr/bin/env node
/* Exercise the actual embedded firmware picker, not a reimplemented algorithm. */
"use strict";
const fs=require("node:fs"),assert=require("node:assert/strict");
function unpack(symbol){
 const source=fs.readFileSync("main/web_device_html.h","utf8");
 const body=source.split("static const char "+symbol+"[] =")[1];
 assert(body,"Missing "+symbol);
 const strings=[];
 for(const line of body.split("\n")){
  const part=line.trim();if(part===";")break;
  if(part)strings.push(JSON.parse(part));
 }
 return strings.join("");
}
const settings=unpack("lf_device_settings_html");
const firmware=unpack("lf_device_firmware_html");
const head=s=>s.match(/<header>[\s\S]*?<\/header>/)?.[0];
assert(head(settings)&&head(firmware),"Settings/Firmware page headers are required");
assert.equal(head(firmware),
 head(settings).replace('settings-link active','settings-link')
               .replace('firmware-link"','firmware-link active"'),
 "Firmware header must match Settings header apart from active tab");
const script=firmware.split("<script>")[1]?.split("</script>")[0];
assert(script,"Embedded firmware JavaScript is missing");
const ids=[...firmware.matchAll(/\bid="([^"]+)"/g)].map(x=>x[1]),elements={};
for(const id of ids)elements[id]={
 value:"",textContent:"",disabled:false,onclick:null,files:[],
 options:[],replaceChildren(...items){this.options=[...items];this.value=items[0]?.value??""},
 add(option){this.options.push(option)}
};
const el=id=>{assert(elements[id],"Unknown page ID: "+id);return elements[id]};
function Option(label,value){this.label=label;this.value=value;this.disabled=false;}
const rawUrl="https://raw.githubusercontent.com/skiiirt1983/LyricFlash/alpha-ota/releases/alpha-build-abc123456789/lyricflash-ota.bin";
let phase=0,requests=[],installed="0.8.4-alpha",confirmations=0;
const makeResponse=data=>({ok:true,json:async()=>data,text:async()=>JSON.stringify(data)});
async function fetchStub(url,opts={}){
 requests.push(url);
 assert.equal(opts.cache,"no-store","Catalog/info requests must bypass cache");
 if(url==="/api/info")return makeResponse({version:installed,ota_result:"idle"});
 if(phase===7)throw Error("Network unavailable");
 if(url.includes("api.github.com/repos/skiiirt1983/LyricFlash/releases")){
   if(phase!==2&&phase!==3&&phase!==4&&phase!==5)return makeResponse([]);
   return makeResponse([{tag_name:"alpha-build-abc123456789",draft:false,prerelease:true,
      published_at:"2026-11-02T22:00:00Z",
      body:"LyricFlash-OTA-Version: 0.8.8-alpha\nLyricFlash-OTA-Direct: "+rawUrl,
      assets:[{name:"lyricflash-ota.bin",size:2100000,
       browser_download_url:"https://github.com/skiiirt1983/LyricFlash/releases/download/alpha-build-abc123456789/lyricflash-ota.bin"}]}]);
 }
 if(url.includes("catalog.json")){
   const versions=phase===0?["0.8.3-alpha","0.7.9-alpha","0.8.4-alpha"]:
     phase===1?["0.8.3-alpha","0.8.5-alpha"]:
     (phase===2||phase===3||phase===4||phase===5)?["0.8.3-alpha","0.8.5-alpha","0.8.8-alpha"]:[];
   return makeResponse({product:"LyricFlash",chipFamily:"ESP32-S3",
     builds:versions.map(version=>({version,label:"BETA "+version,
       url:"https://skiiirt1983.github.io/LyricFlash/beta/firmware/releases/"+version+"/lyricflash-ota.bin"}))});
 }
 if(url.includes("manifest.json"))return makeResponse({version:phase===0?"0.8.4-alpha":
   phase===1?"0.8.5-alpha":"",builds:[{chipFamily:"ESP32-S3"}]});
 throw Error("Unexpected request "+url);
}
const safe=script.replace("loadInstalled().then(loadBuilds);","");
const runtime=new Function("document","fetch","Option","confirm","setTimeout",
 safe+"\nreturn {loadInstalled,loadBuilds,versionCompare,versionParts,selectFirmwareRows,selectedNew,getEntries:()=>entries};")(
 {getElementById:el},fetchStub,Option,()=>{confirmations++;return false;},()=>{});
function rows(){
 return el("builds").options.map(x=>({label:x.label,value:x.value,disabled:x.disabled}));
}
function checkAtMostThree(){
 assert(rows().length<=3,"Picker must never show more than three options");
 assert.deepEqual(rows().map(x=>x.label.split(" — ")[0]).filter(x=>["Old","Current","New"].includes(x)),
  rows().map(x=>x.label.split(" — ")[0]),"Every option must be Old, Current or New");
 for(const r of rows()){
  if(r.label.startsWith("Old — ")||r.label.startsWith("Current — "))
    assert(r.disabled,"Old/Current options must be disabled");
 }
}
(async()=>{
 assert(runtime.versionCompare("0.8.4-alpha","0.8.3-alpha")>0);
 assert(runtime.versionCompare("0.8.5-alpha","0.8.4-alpha")>0);
 assert(runtime.versionCompare("1.0","1.0-beta")>0);
 assert.equal(runtime.versionParts("0.8.4-preview"),null,"Unknown pre-release syntax must fail closed");
 await runtime.loadInstalled();await runtime.loadBuilds();
 checkAtMostThree();
 assert.equal(rows().length,2,"No newer build: only Old and Current");
 assert.match(rows()[0].label,/^Old — 0\.8\.3-alpha/);
 assert.match(rows()[1].label,/^Current — 0\.8\.4-alpha/);
 assert.equal(el("builds").disabled,true,"No New must disable picker");
 assert.equal(el("install").disabled,true);
 el("builds").value="0";el("builds").onchange();
 assert.equal(el("install").disabled,true,"Old must not allow installation");
 await el("install").onclick();
 assert.match(el("fwstatus").textContent,/Select New/i,"Old click must fail closed");
 assert.equal(requests.filter(x=>x==="/api/install").length,0,"No OTA request allowed from Old");
 assert.equal(requests.filter(x=>x.includes("catalog.json")).length,1);

 phase=1;
 await el("refresh-builds").onclick();
 checkAtMostThree();
 assert.equal(rows().length,3);
 assert.match(rows()[0].label,/^Old — 0\.8\.3-alpha/);
 assert.match(rows()[1].label,/^Current — 0\.8\.4-alpha/);
 assert.match(rows()[2].label,/^New — 0\.8\.5-alpha/);
 assert.equal(rows()[2].disabled,false);
 assert.equal(el("builds").value,"1","Default selection is installed version, not New");
 assert.equal(el("install").disabled,true,"No automatic approval of new build");
 el("builds").value="2";el("builds").onchange();
 assert.equal(el("install").disabled,false,"Only selected New enables installation");
 assert.equal(requests.filter(x=>x.includes("catalog.json")).length,2);
 assert.match(el("catalog-status").textContent,/checked again/i);

 phase=2;
 await el("refresh-builds").onclick();
 checkAtMostThree();
 assert.equal(rows().length,3,"Duplicate release formats must dedupe by version");
 assert.match(rows()[2].label,/^New — 0\.8\.8-alpha/);
 assert.equal(runtime.getEntries()[2].url,rawUrl,
  "Verified direct GitHub alpha OTA asset must win for duplicated version");
 assert.equal(el("install").disabled,true,"Changing published version requires re-selection");
 el("builds").value="2";el("builds").onchange();
 assert.equal(el("install").disabled,false);
 await el("install").onclick();
 assert.equal(confirmations,1,"New installation should require manual confirmation");
 assert.equal(requests.filter(x=>x==="/api/install").length,0,"Cancelled confirmation must not download");

 phase=3;installed="0.8.8-alpha";
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,2,"Installed equals latest: Old/Current only");
 assert.match(rows()[0].label,/^Old — 0\.8\.5-alpha/);
 assert.match(rows()[1].label,/^Current — 0\.8\.8-alpha/);
 assert.equal(el("install").disabled,true);

 phase=4;installed="0.6.0-alpha";
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,2,"No older published build: Current/New only");
 assert.match(rows()[0].label,/^Current — 0\.6\.0-alpha/);
 assert.match(rows()[1].label,/^New — 0\.8\.8-alpha/);

 phase=5;installed="legacy-private-build";
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,1,"Unparseable installed version cannot offer New");
 assert.match(rows()[0].label,/^Current — legacy-private-build/);
 assert.equal(el("install").disabled,true);

 phase=6;installed="0.8.8-alpha";
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,1,"Empty catalog still displays Current");
 assert.match(rows()[0].label,/^Current — 0\.8\.8-alpha/);
 assert.equal(el("install").disabled,true);

 phase=7;
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,1,"Unreachable catalog cannot enable updates");
 assert.equal(el("install").disabled,true);
 assert.match(el("catalog-status").textContent,/unavailable/i);

 assert(script.includes("d.ota_detail")&&script.includes("d.ota_http_status")&&
        script.includes("'no_internet','time_unsynced'"),
        "Firmware page must retain progress diagnostics");
 assert(script.includes("xhr.upload.onprogress")&&script.includes("xhr.onerror"),
        "Manual application-only OTA upload must remain intact");
 console.log("PASS: Firmware 3-version picker: old/current/new; no old install, safe default, exact version dedupe, newer-only, missing/invalid/unreachable catalog, direct GitHub alpha image, refresh, OTA diagnostics");
})().catch(e=>{console.error(e);process.exitCode=1});
