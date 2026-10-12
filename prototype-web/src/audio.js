// Aucun fichier sonore : les effets courts sont synthétisés à la demande.
export class Audio {
  constructor() {this.context=null; this.enabled=true;}
  async unlock() {
    if (!this.enabled) return;
    const AudioContext=window.AudioContext||window.webkitAudioContext;
    if (!AudioContext) return;
    if (!this.context) this.context=new AudioContext();
    if (this.context.state==='suspended') await this.context.resume();
  }
  play(kind) {
    if (!this.enabled || !this.context || this.context.state!=='running') return;
    const sounds={shot:[210,65,0.075,'sawtooth',0.025],hit:[560,220,0.065,'square',0.025],
      kill:[260,70,0.17,'triangle',0.07],hurt:[130,42,0.18,'sawtooth',0.035],
      jump:[180,420,0.14,'sine',0.045],dash:[380,80,0.16,'triangle',0.05],
      pickup:[440,880,0.18,'sine',0.06],wave:[330,660,0.35,'triangle',0.045]};
    const [start,end,duration,type,volume]=sounds[kind]||sounds.hit;
    const ctx=this.context,now=ctx.currentTime;
    const oscillator=ctx.createOscillator(),gain=ctx.createGain();
    oscillator.type=type;
    oscillator.frequency.setValueAtTime(start,now);
    oscillator.frequency.exponentialRampToValueAtTime(end,now+duration);
    gain.gain.setValueAtTime(volume,now);
    gain.gain.exponentialRampToValueAtTime(0.001,now+duration);
    oscillator.connect(gain); gain.connect(ctx.destination);
    oscillator.start(now); oscillator.stop(now+duration);
    oscillator.onended=()=>{oscillator.disconnect();gain.disconnect();};
  }
}
