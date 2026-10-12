import { CONFIG } from './config.js';
import { World } from './world.js';
import { Renderer } from './renderer.js';
import { Input } from './input.js';
import { Audio } from './audio.js';
import { Game } from './game.js';

const el=id=>document.getElementById(id);
const canvas=el('game'),menu=el('menu'),hud=el('hud');
let renderer,game,input;
const audio=new Audio();
let state='menu',lastTime=0,accumulator=0,lastRender=0,lastHud=0;
let fpsFrames=0,fpsStart=0,fpsValue=0,contextLost=false;
let best=0;
try {best=Number(localStorage.getItem('fastfps-best'))||0;} catch {}

function pause() {
  if (state!=='running') return;
  state='paused';
  accumulator=0;
  if (document.pointerLockElement===canvas) document.exitPointerLock();
  menu.hidden=false; hud.hidden=true;
  el('play').innerHTML='REPRENDRE <span>→</span>';
  el('restart').hidden=false;
  el('menu-description').textContent='En pause. Reprends la partie quand tu es prêt.';
}

function die() {
  state='dead'; input.clear();
  if (document.pointerLockElement===canvas) document.exitPointerLock();
  menu.hidden=false; hud.hidden=true;
  best=Math.max(best,game.score);
  try {localStorage.setItem('fastfps-best',String(best));} catch {}
  el('result').hidden=false;
  el('result').textContent=`SCORE ${game.score} · VAGUE ${game.wave} · ${game.kills} ROBOTS · RECORD ${best}`;
  el('play').innerHTML='REJOUER <span>→</span>';
  el('restart').hidden=true;
  el('menu-description').textContent='Fin de la partie. Esquive les projectiles orange en te déplaçant et avec le dash.';
}

function loadSettings() {
  try {
    const settings=JSON.parse(localStorage.getItem('fastfps-settings')||'null');
    if (!settings) return;
    if (CONFIG.presets[settings.quality]) el('quality').value=settings.quality;
    if (typeof settings.sensitivity==='number') el('sensitivity').value=Math.max(0.5,Math.min(3,settings.sensitivity));
    if (typeof settings.sound==='boolean') el('sound').checked=settings.sound;
  } catch {}
}

function applySettings() {
  const quality=el('quality').value,sensitivity=Number(el('sensitivity').value),sound=el('sound').checked;
  if (renderer) {
    renderer.setPreset(quality);
    game.particleLimit=renderer.preset.particles;
    input.sensitivity=sensitivity;
    el('presetlabel').textContent=renderer.preset.label;
  }
  audio.enabled=sound;
  el('sensitivity-value').value=sensitivity.toFixed(1);
  try {localStorage.setItem('fastfps-settings',JSON.stringify({quality,sensitivity,sound}));} catch {}
}

function reset() {
  game.reset(); input.player=game.player; input.clear();
  state='ready'; accumulator=0;
  el('result').hidden=true;
  el('restart').hidden=true;
}

async function play() {
  if (!renderer || contextLost) return;
  el('notice').textContent='';
  applySettings();
  if (state==='menu' || state==='dead') reset();
  // La capture est demandée directement dans le geste utilisateur.
  audio.unlock().catch(()=>{el('notice').textContent='Le son est indisponible ; le jeu reste jouable.';});
  try {await input.lock();}
  catch (error) {el('notice').textContent=`Capture de la souris refusée : ${error.message}`;}
}

function updateHud() {
  const p=game.player;
  el('health').textContent=String(Math.ceil(p.health));
  el('healthbar').style.width=`${p.health}%`;
  el('healthbar').style.background=p.health<30?'#ff785f':'#68f5df';
  el('dashbar').style.width=`${100*(1-p.dashCooldown/CONFIG.player.dashCooldown)}%`;
  el('dashlabel').textContent=p.dashCooldown===0?'PRÊT':`${p.dashCooldown.toFixed(1)} s`;
  el('score').textContent=String(game.score).padStart(5,'0');
  el('wave').textContent=String(game.wave).padStart(2,'0');
  el('remaining').textContent=game.enemies.length?`${game.enemies.length} ROBOTS`:'PROCHAINE VAGUE…';
  el('speed').innerHTML=`${Math.round(Math.hypot(p.vx,p.vz))} <span>m/s</span>`;
  el('announcement').textContent=game.messageTime>0?game.message:'';
  el('hitmarker').style.opacity=game.hitTime>0?'1':'0';
  el('damage').style.opacity=String(game.damageTime/0.32);
  el('fps').textContent=`${fpsValue} FPS`;
}

function frame(timestamp) {
  requestAnimationFrame(frame);
  const elapsed=Math.min((timestamp-lastTime)/1000,0.1);
  lastTime=timestamp;
  if (!renderer || contextLost) return;
  if (state==='running') {
    accumulator+=elapsed;
    let steps=0;
    while (accumulator>=CONFIG.fixedStep && steps<CONFIG.maxSteps) {
      game.update(CONFIG.fixedStep,input);
      accumulator-=CONFIG.fixedStep; steps++;
      if (game.dead) {die();break;}
    }
    if (steps===CONFIG.maxSteps) accumulator=0;
  } else accumulator=0;
  // Le menu se rafraîchit seulement à 10 Hz ; la simulation est arrêtée.
  const interval=state==='running'?1000/renderer.preset.fps:100;
  if (timestamp-lastRender>=interval-0.5) {
    lastRender=timestamp;
    renderer.render(game,game.time);
    fpsFrames++;
    if (timestamp-fpsStart>=1000) {
      fpsValue=Math.round(fpsFrames*1000/(timestamp-fpsStart));
      fpsFrames=0; fpsStart=timestamp;
    }
  }
  if (state==='running' && timestamp-lastHud>60) {updateHud();lastHud=timestamp;}
}

try {
  const world=new World();
  game=new Game(world,audio);
  renderer=new Renderer(canvas,world);
  input=new Input(canvas,pause); input.player=game.player;
  loadSettings(); applySettings();
  document.addEventListener('pointerlockchange',()=>{
    if (document.pointerLockElement!==canvas || contextLost) return;
    state='running'; menu.hidden=true; hud.hidden=false;
    lastTime=performance.now(); accumulator=0;
    fpsFrames=0; fpsStart=lastTime;
    updateHud();
  });
  document.addEventListener('pointerlockerror',()=>{
    el('notice').textContent='Le navigateur a refusé la capture de la souris. Clique à nouveau sur le bouton pour réessayer.';
  });
  canvas.addEventListener('webglcontextlost',event=>{
    event.preventDefault(); contextLost=true; pause();
    menu.hidden=false; hud.hidden=true;
    el('play').disabled=true;
    el('notice').textContent='Le contexte graphique a été perdu. Recharge la page pour relancer le jeu.';
  });
  el('play').addEventListener('click',play);
  el('restart').addEventListener('click',()=>{reset();play();});
  for (const id of ['quality','sensitivity','sound']) el(id).addEventListener('input',applySettings);
  el('fullscreen').addEventListener('click',async()=>{
    try {
      if (document.fullscreenElement) await document.exitFullscreen();
      else await document.documentElement.requestFullscreen();
    } catch {el('notice').textContent='Le plein écran est indisponible dans ce navigateur.';}
  });
  requestAnimationFrame(frame);
} catch (error) {
  console.error(error);
  el('play').disabled=true;
  el('notice').textContent=`Impossible de démarrer : ${error.message}`;
}
