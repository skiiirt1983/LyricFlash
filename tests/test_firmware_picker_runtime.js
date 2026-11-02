#!/usr/bin/env node
/* Exercise the real embedded Firmware Web UI without firmware compilation. */
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
assert(head(settings)&&head(firmware),"Both pages must have headers");
assert.equal(head(firmware),
 head(settings).replace('settings-link active','settings-link')
               .replace('firmware-link"','firmware-link active"'),
 "Firmware header must exactly match Settings header except active tab");
const script=firmware.split("<script>")[1]?.split("</script>")[0];
assert(script,"Firmware script missing");
const htmlIds=[...firmware.matchAll(/\bid="([^"]+)"/g)].map(x=>x[1]);
const elements={};
for(const id of htmlIds)elements[id]={
 value:"",textContent:"",disabled:false,onclick:null,files:[],
 options:[],replaceChildren(...items){this.options=[...items];this.value=items[0]?.value??""},
 add(option){this.options.push(option)}
};
const el=id=>{assert(elements[id],"Missing DOM id "+id);return elements[id]};
function Option(label,value){this.label=label;this.value=value;}
let phase=0,requests=[],installed="0.8.4-alpha";
const makeResponse=data=>({ok:true,json:async()=>data,text:async()=>JSON.stringify(data)});
async function fetchStub(url,opts={}){
 requests.push(url);assert.equal(opts.cache,"no-store","Catalog must bypass browser cache");
 if(url==="/api/info")return makeResponse({version:installed,ota_result:"idle"});
 if(url.includes("api.github.com/repos/skiiirt1983/LyricFlash/releases")){
   if(phase!==2)return makeResponse([]);
   return makeResponse([{tag_name:"alpha-build-abc123456789",draft:false,prerelease:true,
      published_at:"2026-11-02T22:00:00Z",body:"LyricFlash-OTA-Version: 0.8.8-alpha\\nLyricFlash-OTA-Direct: https://raw.githubusercontent.com/skiiirt1983/LyricFlash/alpha-ota/releases/alpha-build-abc123456789/lyricflash-ota.bin",
      assets:[{name:"lyricflash-ota.bin",size:2100000,
       browser_download_url:"https://github.com/skiiirt1983/LyricFlash/releases/download/alpha-build-abc123456789/lyricflash-ota.bin"}]}]);
 }
 if(url.includes("catalog.json")){
  const versions=phase===0?["0.8.3-alpha","0.7.9-alpha","0.8.4-alpha"]:["0.8.3-alpha","0.8.5-alpha"];
  return makeResponse({product:"LyricFlash",chipFamily:"ESP32-S3",
   builds:versions.map(version=>({version,label:"BETA "+version,
    url:"https://skiiirt1983.github.io/LyricFlash/beta/firmware/releases/"+version+"/lyricflash-ota.bin"}))});
 }
 if(url.includes("manifest.json"))return makeResponse({
  version:phase===0?"0.8.4-alpha":"0.8.5-alpha",builds:[{chipFamily:"ESP32-S3"}]});
 throw Error("Unexpected request "+url);
}
const safe=script.replace("loadInstalled().then(()=>loadBuilds());","")
                 .replace("loadInstalled().then(loadBuilds);","");
const runtime=new Function("document","fetch","Option","confirm","setTimeout",
 safe+"\nreturn {loadInstalled,loadBuilds,versionCompare,getEntries:()=>entries};")(
 {getElementById:el},fetchStub,Option,()=>false,()=>{});
(async()=>{
 assert(runtime.versionCompare("0.8.4-alpha","0.8.3-alpha")>0);
 assert(runtime.versionCompare("0.8.5-alpha","0.8.4-alpha")>0);
 assert(runtime.versionCompare("1.0","1.0-beta")>0);
 await runtime.loadInstalled();await runtime.loadBuilds();
 assert.match(el("builds").options[1].label,/0\.8\.4-alpha.*INSTALLED/);
 assert.match(el("builds").options[2].label,/0\.8\.3-alpha.*OLDER THAN INSTALLED/);
 assert(!el("builds").options.some(x=>/ — NEW/.test(x.label)),"Oldest build falsely labelled NEW");
 el("builds").value="1";el("builds").onchange();
 assert.equal(el("install").disabled,true,"Older build downgrade was not blocked");
 const fetched=requests.filter(x=>x.includes("catalog.json"));
 assert.equal(fetched.length,1);
 phase=1;
 await el("refresh-builds").onclick();
 assert.equal(requests.filter(x=>x.includes("catalog.json")).length,2,
  "Refresh did not request the GitHub OTA catalog again");
 assert.match(el("builds").options[1].label,/0\.8\.5-alpha.*LATEST PUBLISHED/);
 assert.match(el("catalog-status").textContent,/checked again/i);
 assert.equal(el("refresh-builds").disabled,false);
 el("builds").value="0";el("builds").onchange();
 assert.equal(el("install").disabled,false,"Valid newer build should be selectable");
 phase=2;
 await el("refresh-builds").onclick();
 assert.match(el("builds").options[1].label,/0\.8\.8-alpha.*LATEST DEVELOPMENT/);
 assert.equal(runtime.getEntries()[0].url,
 "https://raw.githubusercontent.com/skiiirt1983/LyricFlash/alpha-ota/releases/alpha-build-abc123456789/lyricflash-ota.bin",
 "Picker must prefer a verified direct asset URL");
 el("builds").value="0";el("builds").onchange();
 assert.equal(el("install").disabled,false,"Alpha prerelease build must be selectable");
 assert(el("build-info").textContent.includes("alpha development builds"));
 assert(script.includes("d.ota_detail")&&script.includes("d.ota_http_status")&&
        script.includes("'no_internet','time_unsynced'"),
        "OTA progress diagnostics must surface errors and stop polling");
 console.log("PASS: matching Settings/Firmware headers, sorted OTA builds, safe old-build handling, live GitHub refresh and OTA diagnostics");
})().catch(e=>{console.error(e);process.exitCode=1});
