export class Input {
  constructor(canvas,onPause) {
    this.canvas=canvas; this.keys=new Set(); this.firing=false;
    this.sensitivity=1; this.player=null; this.locked=false;
    document.addEventListener('keydown',event=>{
      if (!this.locked) return;
      if (['Space','ArrowUp','ArrowDown','ArrowLeft','ArrowRight','ShiftLeft','ShiftRight'].includes(event.code)) event.preventDefault();
      this.keys.add(event.code);
    });
    document.addEventListener('keyup',event=>this.keys.delete(event.code));
    document.addEventListener('mousemove',event=>{
      if (!this.locked || !this.player) return;
      const factor=0.002*this.sensitivity;
      this.player.yaw=(this.player.yaw+event.movementX*factor)%(Math.PI*2);
      this.player.pitch=Math.max(-1.45,Math.min(1.45,this.player.pitch-event.movementY*factor));
    });
    document.addEventListener('mousedown',event=>{
      if (this.locked && event.button===0) this.firing=true;
    });
    document.addEventListener('mouseup',event=>{if (event.button===0) this.firing=false;});
    canvas.addEventListener('contextmenu',event=>event.preventDefault());
    document.addEventListener('pointerlockchange',()=>{
      this.locked=document.pointerLockElement===canvas;
      this.clear();
      if (!this.locked) onPause();
    });
    window.addEventListener('blur',()=>{this.clear(); onPause();});
    document.addEventListener('visibilitychange',()=>{if (document.hidden) {this.clear(); onPause();}});
  }
  clear() {this.keys.clear(); this.firing=false;}
  async lock() {
    if (!this.canvas.requestPointerLock) throw new Error('Ce navigateur ne permet pas la capture de la souris. Utilise Firefox ou Chromium sur ordinateur.');
    // Les anciens navigateurs ne retournent pas de Promise.
    await this.canvas.requestPointerLock();
  }
  get forward() {return Number(this.has('KeyW','KeyZ','ArrowUp'))-Number(this.has('KeyS','ArrowDown'));}
  get right() {return Number(this.has('KeyD','ArrowRight'))-Number(this.has('KeyA','KeyQ','ArrowLeft'));}
  get jump() {return this.has('Space');}
  get dash() {return this.has('ShiftLeft','ShiftRight');}
  has(...keys) {return keys.some(key=>this.keys.has(key));}
}
