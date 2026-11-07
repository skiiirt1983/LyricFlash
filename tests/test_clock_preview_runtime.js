#!/usr/bin/env node
/* Exercise the REAL embedded Clock Studio script with a synthetic DOM.
 * No ESP-IDF build, no network or browser dependencies required.
 */
"use strict";
const fs=require("node:fs");
const assert=require("node:assert/strict");
const source=fs.readFileSync("main/web_basic_html.h","utf8");
const body=source.split("static const char lf_basic_settings_html[] =")[1];
assert(body,"Clock Studio HTML missing");
const lines=body.split("\n"),parts=[];
for(const line of lines){
 const v=line.trim();
 if(v===";")break;
 if(v)parts.push(JSON.parse(v));
}
const html=parts.join("");
const js=html.split("<script>")[1]?.split("</script>")[0];
assert(js,"Embedded Clock Studio script missing");
const elements={};
for(const m of html.matchAll(/<([a-z0-9-]+)\b([^>]*\bid="([^"]+)"[^>]*)>/gi)){
 const [,tag,attr,id]=m;
 const type=/type="([^"]+)"/.exec(attr)?.[1]||"";
 assert(!elements[id],"Duplicate HTML id "+id);
 elements[id]={type,tagName:tag.toUpperCase(),
  value:type==="color"?"#ffffff":type==="number"?"50":tag==="select"?"0":"",
  checked:false,hidden:false,innerHTML:"",textContent:"",style:{},dataSpectrum:/data-spectrum="1"/.test(attr),listeners:{},
  addEventListener(type,fn){this.listeners[type]=fn},
  dispatch(type){this.listeners[type]?.({})},onclick:null};
}
const el=id=>{assert(elements[id],"Undeclared HTML id "+id);return elements[id];};
const colours=[0x070f14,0x030815,0x080c13,0xa7c7a0,0xc8bb94,0x030810];
const fronts=[0xd8ece9,0xedf7ff,0xe5edf8,0x162c1f,0x332719,0xf0f8ff];
const settings={
 clock_face:0,weather_enabled:true,city:"Big White",interval_minutes:30,
 font_style:0,font_size:60,ring_mode:1,show_date:true,weather:true,
 background:0x030810,foreground:0xf0f8ff,ring_a:0x268bff,
 ring_b:0x8e48ff,ring_c:0x20c9d4,
 face_profiles:colours.map((v,i)=>({background:v,foreground:fronts[i],
 accent:0x40c0e6,show_date:true,show_seconds:true,show_battery:true,time_24h:false,
 show_percent:true,battery_size:i===5?85:60,date_size:i===5?70:60,
 battery_x:0,battery_y:[-137,-146,-109,-167,-172,-160][i],
 date_x:[99,0,0,0,0,0][i],date_y:[-2,72,106,-53,135,70][i]}))
};
const posts=[];
const sourceFonts=fs.readFileSync("main/basic_open_face.h","utf8");
const bitmapMatch=sourceFonts.match(/static const char lf_open_face_b64\[\]="([A-Za-z0-9+/]+)"/);
assert(bitmapMatch,"Device glyphs missing");
const deviceFonts={digits:bitmapMatch[1],width:15,height:21,families:7};
const fakeFetch=async(url,opts)=>opts?.method==="POST"
 ? (posts.push(JSON.parse(opts.body)),{ok:true,text:async()=>"{\"saved\":true}"})
 : {ok:true,text:async()=>JSON.stringify(String(url).includes('/api/clock-fonts')?deviceFonts:settings)};
const runtime=new Function("document","fetch","setInterval",
 js+"\nreturn {load,save,render,selectPart};")(
 {getElementById:el,querySelectorAll:selector=>selector==="input[data-spectrum]"?Object.values(elements).filter(e=>e.dataSpectrum):[]},fakeFetch,()=>0
);
(async()=>{
 await runtime.load();
 await new Promise(resolve=>setImmediate(resolve)); /* allow font API parse */
 assert.match(el("status").textContent,/loaded/i);
 for(let i=0;i<6;i++){
  el("clock_face").value=String(i);
  el("clock_face").dispatch("change");
  const graphic=el("clock-preview").innerHTML;
  assert(graphic.length>150, "Missing live SVG for "+i);
  assert(graphic.includes("#"+colours[i].toString(16).padStart(6,"0")),
    "Face "+i+" preview has no CSS-legal dial colour");
 }
 el("clock_face").value="5";el("clock_face").dispatch("change");
 const fontPreviews=[];
 for(let i=0;i<24;i++){
  el("font_style").value=String(i);
  el("font_style").dispatch("input");
  fontPreviews.push(el("clock-preview").innerHTML);
 }
 const seenGlyphs=new Map();
 fontPreviews.forEach((markup,id)=>{
  if(seenGlyphs.has(markup))console.log("DUPLICATE PREVIEW FONTS",seenGlyphs.get(markup),id);
  else seenGlyphs.set(markup,id);
 });
 console.log("DIAGNOSTIC FONT UNIQUE",seenGlyphs.size);
 assert.equal(seenGlyphs.size,24,
   "All twenty-four original clock typography options must render distinct previews");
 assert.match(fontPreviews[3],/data-face-style|<rect/,
   "Actual Garamond-like raster glyphs missing from browser preview");
 assert.notEqual(fontPreviews[3],fontPreviews[2],
   "Traditional serif face must differ from Retro 8");
 assert.notEqual(fontPreviews[5],fontPreviews[6],
   "Heavy display must differ from rounded geometric digits");
 /* New font families must produce genuinely distinct SVG preview selections. */
 const requestedFonts=["Kablammo","Mystery Quest","Moirai One","Rampart One",
  "Hanalei","Bitcount Grid Double","Iceberg","Montserrat","Sancreek",
  "Modak","Alumni Sans Collegiate One","Honk","Nabla"];
 const newFontPreviews=[];
 for(let i=0;i<requestedFonts.length;i++){
  el("font_style").value=String(24+i);
  el("font_style").dispatch("input");
  const svg=el("clock-preview").innerHTML;
  assert(svg.includes(requestedFonts[i]),"Missing font in preview: "+requestedFonts[i]);
  newFontPreviews.push(svg);
 }
 assert.equal(new Set(newFontPreviews).size,requestedFonts.length,
  "Requested font choices are not distinguishable in Clock Studio preview");
 el("font_style").value="0";el("font_style").dispatch("input");
 el("font_size").value="0";el("font_size").dispatch("input");
 const smallestTime=el("clock-preview").innerHTML;
 el("font_size").value="200";el("font_size").dispatch("input");
 const largestTime=el("clock-preview").innerHTML;
 assert.notEqual(smallestTime,largestTime,
   "Clock time must actually render at radically different sizes from 0 to 200%");
 el("font_size").value="60";el("font_size").dispatch("input");
 el("basic_battery_size").value="200";el("basic_battery_size").dispatch("input");
 assert.match(el("clock-preview").innerHTML,/75%/,
   "Largest battery preview disappeared");

 const oldBatteryPreview=el("clock-preview").innerHTML;
 el("basic_battery_x").value="37";el("basic_battery_x").dispatch("input");
 const newBatteryPreview=el("clock-preview").innerHTML;
 assert.notEqual(oldBatteryPreview,newBatteryPreview,"Battery position slider does not update preview");
 el("basic_date_size").value="93";el("basic_date_size").dispatch("input");
 assert.equal(el("value_basic_date_size").textContent,"93","Slider value is not shown");
 el("ring_mode").value="0";el("ring_mode").dispatch("input");
 assert.equal(el("ring-b-row").hidden,true,"Disabled ring must hide colour 2");
 assert.equal(el("ring-c-row").hidden,true,"Disabled ring must hide colour 3");
 assert.ok(!el("clock-preview").innerHTML.includes("basic-gradient"),"Ring must not render in clock preview");
 el("clock_face").value="2";el("clock_face").dispatch("change");
 /* Simulate a pointer dragging Classic Analog's battery 39 px right.
    Save must contain new face-local battery coordinates, not Basic's. */
 const previewBox=el("clock-preview");
 previewBox.getBoundingClientRect=()=>({width:466,height:466});
 previewBox.setPointerCapture=()=>{};
 const hit={getAttribute:id=>id==="data-part"?"battery":null,closest:()=>hit};
 const evt=(x,y)=>({target:hit,pointerId:41,clientX:x,clientY:y,preventDefault(){}});
 const first=previewBox.innerHTML;
 previewBox.listeners.pointerdown(evt(110,120));
 previewBox.listeners.pointermove(evt(149,120));
 previewBox.listeners.pointerup(evt(149,120));
 assert.notEqual(previewBox.innerHTML,first,
   "Dragging the Classic battery did not update its preview");
 el("face_background").value="#112233";el("face_background").dispatch("input");
 assert(el("clock-preview").innerHTML.includes('fill="#112233"'),
   "Palette editing does not change SVG preview");
 assert.equal(el("face_background").style.backgroundColor,"#112233",
   "Colour input must render the actual selected swatch");
 assert.equal(el("basic-options").hidden,true,
   "Legacy Basic Custom controls must never appear as a second editor");
 /* Clock element colours are independent per face, with Time including
    the analog hands and Battery including both icon and percentage. */
 runtime.selectPart("time");
 assert.equal(el("selected-color-label").textContent,"Time font colour");
 el("selected-color").value="#ff33cc";el("selected-color").dispatch("input");
 assert(el("clock-preview").innerHTML.includes("#ff33cc"),
   "Time colour did not affect analog hands");
 runtime.selectPart("battery");
 el("selected-color").value="#22ddaa";el("selected-color").dispatch("input");
 assert.equal(el("selected-color-label").textContent,"Battery icon and % colour");
 assert(el("clock-preview").innerHTML.includes("#22ddaa"),
   "Battery colour did not change");
 assert.equal(el("selected-color").value,"#22ddaa");
 runtime.selectPart("time");
 assert.equal(el("selected-color").value,"#ff33cc",
   "Battery colour incorrectly overwrote Time colour");
 el("clock_face").value="3";el("clock_face").dispatch("change");
 assert(el("clock-preview").innerHTML.includes("#a7c7a0"),
   "Changing one face overwrote another");
 el("clock_face").value="2";el("clock_face").dispatch("change");
 assert.equal(el("face_background").value,"#112233",
   "Face profile discarded on face switch");
 await runtime.save();
 assert.equal(posts.length,1,"Clock Studio did not send save request");
 assert.equal(posts[0].face_profiles.length,6);
 assert.equal(posts[0].basic_battery_x,37,"Battery X not saved as number");
 assert.equal(posts[0].basic_date_size,93,"Date size not saved as number");
 assert.equal(posts[0].face_profiles[2].background,0x112233);
 assert.equal(posts[0].face_profiles[2].time_color,0xff33cc);
 assert.equal(posts[0].face_profiles[2].battery_color,0x22ddaa);
 assert.notEqual(posts[0].face_profiles[3].time_color,0xff33cc);
 assert.equal(posts[0].face_profiles[2].battery_x,39,
    "Non-Basic battery drag was not included in saved face profile");
 assert.equal(posts[0].face_profiles[2].battery_y,-109,
    "Moving battery horizontally corrupted the vertical position");
 assert.equal(posts[0].face_profiles[3].background,0xa7c7a0);
 el("reset-face").onclick();
 assert.equal(el("face_background").value,"#080c13",
   "Reset selected face failed");
 el("clock_face").value="5";el("clock_face").dispatch("change");
 el("basic_time_x").value="84";
 el("font_size").value="120";
 el("reset-face").onclick();
 assert.equal(el("basic_time_x").value,"0","Basic geometry not reset");
 assert.equal(el("font_size").value,"60","Basic time size not reset");
 assert.equal(el("basic-options").hidden,true,"Second Basic control panel became visible");
 console.log("PASS: 24 distinct numeral previews, draggable battery on Classic, saved per-face positions, six clock faces");
})().catch(err=>{console.error(err);process.exitCode=1});
