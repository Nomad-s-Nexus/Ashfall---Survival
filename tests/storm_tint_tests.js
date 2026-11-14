const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const view=path.join(__dirname,'../deploy/PrismaUI_F4/views/Interface/FalloutAnomaly');
const source=fs.readFileSync(path.join(view,'radhud.js'),'utf8');
function element(){return {style:{},className:'',textContent:''};}
const elements={emissionBanner:element(),emissionSub:element(),stormTint:element()};
const context={document:{getElementById:id=>elements[id]||null},setTimeout(){return 0;},clearTimeout(){}};
context.window=context;
vm.createContext(context);
vm.runInContext(source,context);

const tint=(theme,phase,intensity,sheltered)=>context.stormTintFor(theme,phase,intensity,sheltered);
const rgb=t=>[t.r,t.g,t.b];
const EXPECTED={1:[255,70,20],2:[225,200,50],3:[80,200,90],4:[255,255,255],5:[140,100,55]};
const THEMES=[1,2,3,4,5];

for(const theme of THEMES){
  const t=tint(theme,2,1,false);
  assert.deepEqual(rgb(t),EXPECTED[theme],'theme '+theme+' colour');
  assert.ok(t.a>0,'theme '+theme+' must be visible while Active and exposed');
}

const caps={};
for(const theme of THEMES)caps[theme]=tint(theme,2,1,false).a;
for(const theme of THEMES)assert.ok(caps[theme]>=0.10&&caps[theme]<=0.22,'alpha cap out of range for theme '+theme+': '+caps[theme]);
assert.ok(caps[4]<Math.min(caps[1],caps[5]),'white ash must be dimmer than red firestorm and brown dust');
assert.ok(caps[2]<Math.min(caps[1],caps[5]),'yellow sulfur must be dimmer than red firestorm and brown dust');

for(const theme of THEMES){
  assert.ok(Math.abs(tint(theme,1,0.35,false).a-caps[theme]*0.35)<1e-9,'Warning must use 0.35x intensity for theme '+theme);
  assert.ok(Math.abs(tint(theme,3,0.25,false).a-caps[theme]*0.25)<1e-9,'Aftermath must scale with its intensity for theme '+theme);
}

for(const theme of [0,1,2,3,4,5]){
  assert.equal(tint(theme,0,1,false).a,0,'Idle must be hidden for theme '+theme);
  assert.equal(tint(theme,4,1,false).a,0,'Approach must be hidden for theme '+theme);
  assert.equal(tint(theme,2,1,true).a,0,'sheltered Active must be hidden for theme '+theme);
  assert.equal(tint(theme,1,0.35,true).a,0,'sheltered Warning must be hidden for theme '+theme);
}
assert.equal(tint(0,2,1,false).a,0,'theme 0 has no tint');
for(const bad of [0,-1,NaN,undefined])assert.equal(tint(1,2,bad,false).a,0,'intensity '+bad+' must hide the tint');
assert.equal(tint(1,2,5,false).a,caps[1],'intensity above 1 must clamp to the cap');

context.stormPhase({phase:2,theme:4,intensity:1,sheltered:false,lethal:true});
assert.equal(elements.stormTint.style.backgroundColor,'rgb(255,255,255)');
assert.equal(elements.stormTint.style.opacity,String(caps[4]));
context.stormPhase(JSON.stringify({phase:2,theme:4,intensity:1,sheltered:true,lethal:true}));
assert.equal(elements.stormTint.style.opacity,'0');
context.stormPhase({phase:1,theme:1,intensity:0.35,sheltered:false,lethal:true});
assert.equal(elements.stormTint.style.opacity,String(caps[1]*0.35));
assert.equal(elements.stormTint.style.backgroundColor,'rgb(255,70,20)');
context.stormPhase({phase:0,theme:1,intensity:0,sheltered:false});
assert.equal(elements.stormTint.style.opacity,'0');
context.stormPhase({phase:4,theme:0,intensity:0,sheltered:false});
assert.equal(elements.stormTint.style.opacity,'0');

delete elements.stormTint;
context.stormPhase({phase:2,theme:1,intensity:1,sheltered:false,lethal:true});
assert.equal(elements.emissionBanner.className,'on fire-b','the banner must still update when the tint element is missing');

console.log('Storm tint verified: per-theme colours, Warning/Aftermath scaling, alpha caps, hidden when Idle/Approach/sheltered, and stormPhase wiring');
