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
 if(phase===8&&(url.includes("catalog.json")||url.includes("manifest.json")))
   return {ok:false,status:404,json:async()=>({})};
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
 safe+"\nreturn {loadInstalled,loadBuilds,versionCompare,versionParts,selectFirmwareRows,selectedInstall,getEntries:()=>entries};")(
 {getElementById:el},fetchStub,Option,()=>{confirmations++;return false;},()=>{});
function rows(){
 return el("builds").options.map(x=>({label:x.label,value:x.value,disabled:x.disabled}));
}
function checkAtMostThree(){
 assert(rows().length<=3,"Picker must never show more than three options");
 assert.deepEqual(rows().map(x=>x.label.split(" — ")[0]).filter(x=>["Old","Current","New"].includes(x)),
  rows().map(x=>x.label.split(" — ")[0]),"Every option must be Old, Current or New");
 for(const r of rows()){
  if(r.label.startsWith("Old — ")) assert(r.disabled,"Old firmware must never be installable");
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
 assert.equal(el("builds").disabled,false,"Published Current should enable picker even when New is unavailable");
 assert.equal(rows()[1].disabled,false,"A matching, published Current is reinstallable");
 assert.equal(el("install").disabled,false,"Current reinstall button must be available");
 assert.match(el("install").textContent,/Reinstall current firmware/);
 assert.equal(runtime.selectedInstall().kind,"Current");
 await el("install").onclick();
 assert.equal(confirmations,1,"Reinstall requires confirmation");
 assert.equal(requests.filter(x=>x==="/api/install").length,0,"Cancelled reinstall must not flash");
 el("builds").value="0";el("builds").onchange();
 assert.equal(el("install").disabled,true,"Old must not allow installation");
 await el("install").onclick();
 assert.match(el("fwstatus").textContent,/Only a verified Current reinstall or a New update/i,"Old click must fail closed");
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
 assert.equal(rows()[1].disabled,true,"Current must be disabled if its exact published image is unavailable");
 assert.equal(el("install").disabled,true,"No automatic approval of new build");
 el("builds").value="2";el("builds").onchange();
 assert.equal(el("install").disabled,false,"Only selected New enables installation");
 assert.equal(requests.filter(x=>x.includes("catalog.json")).length,2);
 assert.match(el("catalog-status").textContent,/Update check complete/i);

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
 assert.equal(confirmations,2,"New installation should require its own confirmation");
 assert.equal(requests.filter(x=>x==="/api/install").length,0,"Cancelled confirmation must not download");

 phase=3;installed="0.8.8-alpha";
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,2,"Installed equals latest: Old/Current only");
 assert.match(rows()[0].label,/^Old — 0\.8\.5-alpha/);
 assert.match(rows()[1].label,/^Current — 0\.8\.8-alpha/);
 assert.equal(el("install").disabled,false,"Current must remain reinstallable when no newer release exists");
 assert.equal(rows()[1].disabled,false);
 assert.match(el("install").textContent,/Reinstall current firmware/);
 assert.equal(runtime.selectedInstall().version,"0.8.8-alpha");
 await el("install").onclick();
 assert.equal(confirmations,3,"Current version reinstall needs explicit confirmation");
 assert.equal(requests.filter(x=>x==="/api/install").length,0);

 phase=4;installed="0.6.0-alpha";
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,2,"No older published build: Current/New only");
 assert.match(rows()[0].label,/^Current — 0\.6\.0-alpha/);
 assert.equal(rows()[0].disabled,true,"Unpublished currently running image cannot be reinstalled");
 assert.match(rows()[1].label,/^New — 0\.8\.8-alpha/);

 phase=5;installed="legacy-private-build";
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,1,"Unparseable installed version cannot offer New");
 assert.match(rows()[0].label,/^Current — legacy-private-build/);
 assert.equal(rows()[0].disabled,true);
 assert.equal(el("install").disabled,true);

 phase=6;installed="0.8.8-alpha";
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,1,"Empty catalog still displays Current");
 assert.match(rows()[0].label,/^Current — 0\.8\.8-alpha/);
 assert.equal(rows()[0].disabled,true,"Unpublished Current must not have a guessed OTA URL");
 assert.equal(el("install").disabled,true);

 phase=7;
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,1,"Unreachable catalog cannot enable updates");
 assert.equal(el("install").disabled,true);
 assert.match(el("catalog-status").textContent,/Unable to check for updates/i);
 assert.doesNotMatch(el("catalog-status").textContent,/HTTP|404|Catalog:|Some sources unavailable/i);

 phase=8;installed="0.8.8-alpha";
 await el("refresh-builds").onclick();checkAtMostThree();
 assert.equal(rows().length,1,"Current remains visible when optional Pages endpoints 404");
 assert.match(el("catalog-status").textContent,/No newer firmware available/i);
 assert.doesNotMatch(el("catalog-status").textContent,/HTTP|404|Catalog:|Some sources unavailable/i,
  "Optional GitHub Pages catalog HTTP 404 must never be displayed to end user");


 assert(script.includes("d.ota_detail")&&script.includes("d.ota_http_status")&&
        script.includes("'no_internet','time_unsynced'"),
        "Firmware page must retain progress diagnostics");
 assert(script.includes("xhr.upload.onprogress")&&script.includes("xhr.onerror"),
        "Manual application-only OTA upload must remain intact");
 const source=fs.readFileSync("main/network.c","utf8");
 assert(source.includes('strcmp(descriptor.version,job->version)!=0'),
        "Device must validate the downloaded image is the selected version");
 assert(source.includes('esp_https_ota_finish(handle)'),
        "Device must validate firmware before activating it");
 assert(!source.includes('strcmp(job->version,LF_VERSION)==0'),
        "Device must not reject reinstallation of the same version");
 assert(script.includes("reinstall is NOT a factory reset"),
        "Confirmation must not misrepresent reinstall as factory reset");
 console.log("PASS: friendly optional-catalog 404 status, 3-version OTA picker: Old disabled, verified Current reinstallable, New selectable, matching OTA descriptor, safe missing/current-only/empty catalogs, explicit confirmation, refresh and diagnostics");
})().catch(e=>{console.error(e);process.exitCode=1});
