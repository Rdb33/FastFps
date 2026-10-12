import { CONFIG } from './config.js';
import { direction, rayBox } from './math.js';
import { createPlayer, updatePlayer, moveHorizontal, moveVertical } from './physics.js';

export class Game {
  constructor(world,audio) {
    this.world=world; this.audio=audio; this.particleLimit=28;
    this.reset();
  }
  reset() {
    this.player=createPlayer();
    this.enemies=[]; this.projectiles=[]; this.particles=[];
    this.pickups=[{x:-23,z:-18},{x:23,z:-18},{x:-23,z:18},{x:23,z:18}].map(p=>({...p,cooldown:0}));
    this.wave=0; this.score=0; this.kills=0; this.time=0;
    this.waveTimer=1.5; this.navTimer=0; this.shotCooldown=0;
    this.recoil=0; this.hitTime=0; this.damageTime=0;
    this.message='BOUGE POUR SURVIVRE'; this.messageTime=2;
    this.dead=false; this.nextId=0;
    this.world.navigation.update(this.player.x,this.player.z);
  }
  say(message,duration=2) {this.message=message;this.messageTime=duration;}
  startWave() {
    this.wave++;
    const count=Math.min(CONFIG.enemy.maxAlive,3+this.wave*2);
    const spawns=[...this.world.spawns].sort(()=>Math.random()-0.5);
    const far=spawns.filter(s=>Math.hypot(s.x-this.player.x,s.z-this.player.z)>10);
    const choices=far.length?far:spawns;
    for (let i=0;i<count;i++) {
      const spawn=choices[i%choices.length];
      // Les positions sont espacées même lorsque plusieurs robots utilisent une entrée.
      const offset=Math.floor(i/choices.length)*1.5;
      const x=spawn.x+(spawn.x>0?-offset:offset),z=spawn.z;
      this.enemies.push({id:this.nextId++,x,y:0,z,vx:0,vy:0,vz:0,grounded:true,
        health:CONFIG.enemy.health,flash:0,shotTimer:0.7+Math.random()*1.5,touchTimer:0});
    }
    this.waveTimer=-1;
    this.say(`VAGUE ${String(this.wave).padStart(2,'0')}`);
    this.audio.play('wave');
  }
  burst(x,y,z,color,count=6) {
    for (let i=0;i<count && this.particles.length<this.particleLimit;i++) {
      const life=0.15+Math.random()*0.25;
      this.particles.push({x,y,z,vx:(Math.random()-0.5)*5,vy:Math.random()*4,
        vz:(Math.random()-0.5)*5,life,maxLife:life,size:0.07+Math.random()*0.06,color});
    }
  }
  shoot() {
    const p=this.player,c=CONFIG.weapon;
    const origin={x:p.x,y:p.y+CONFIG.player.eye,z:p.z};
    const dir=direction(p.yaw,p.pitch);
    let distance=this.world.ray(origin,dir,c.range),victim=null;
    for (const bot of this.enemies) {
      const hit=rayBox(origin,dir,{x0:bot.x-0.48,x1:bot.x+0.48,y0:bot.y,y1:bot.y+CONFIG.enemy.height,
        z0:bot.z-0.42,z1:bot.z+0.42},distance);
      if (hit<distance) {distance=hit;victim=bot;}
    }
    const hit={x:origin.x+dir.x*distance,y:origin.y+dir.y*distance,z:origin.z+dir.z*distance};
    this.burst(hit.x,hit.y,hit.z,victim?[1,0.46,0.18]:[0.3,0.9,0.78],victim?6:3);
    for (let i=1;i<=5 && this.particles.length<this.particleLimit;i++) {
      const t=distance*i/6;
      this.particles.push({x:origin.x+dir.x*t,y:origin.y+dir.y*t,z:origin.z+dir.z*t,
        vx:0,vy:0,vz:0,life:0.065,maxLife:0.065,size:0.025,color:[0.4,1,0.85]});
    }
    if (victim) {
      victim.health-=c.damage; victim.flash=0.10; this.hitTime=0.12;
      if (victim.health<=0) {
        this.score+=100+this.wave*10; this.kills++;
        this.burst(victim.x,victim.y+0.9,victim.z,[1,0.6,0.22],10);
        this.audio.play('kill');
      } else this.audio.play('hit');
    }
    this.recoil=1; this.shotCooldown=c.interval; this.audio.play('shot');
  }
  hurt(amount) {
    if (this.dead) return;
    this.player.health=Math.max(0,this.player.health-amount);
    this.damageTime=0.32; this.audio.play('hurt');
    if (this.player.health===0) this.dead=true;
  }
  updateEnemies(dt) {
    const p=this.player,c=CONFIG.enemy;
    for (const bot of this.enemies) {
      if (bot.health<=0) continue;
      bot.flash=Math.max(0,bot.flash-dt);
      bot.shotTimer-=dt; bot.touchTimer=Math.max(0,bot.touchTimer-dt);
      const dx=p.x-bot.x,dz=p.z-bot.z;
      const distance=Math.hypot(dx,dz);
      const origin={x:bot.x,y:bot.y+1.35,z:bot.z};
      const dy=p.y+CONFIG.player.eye-origin.y;
      const targetDistance=Math.hypot(dx,dy,dz);
      const dir={x:dx/Math.max(targetDistance,0.001),y:dy/Math.max(targetDistance,0.001),z:dz/Math.max(targetDistance,0.001)};
      const visible=this.world.ray(origin,dir,targetDistance)>=targetDistance-0.1;
      let target=visible?{x:p.x,z:p.z}:this.world.navigation.next(bot.x,bot.z);
      let tx=target.x-bot.x,tz=target.z-bot.z;
      // Garder une distance de tir et se déplacer latéralement.
      if (visible && distance<8 && distance>2) {
        const sign=bot.id%2?1:-1;
        tx=-dz*sign; tz=dx*sign;
      }
      const length=Math.hypot(tx,tz);
      const speed=Math.min(6.2,c.speed+(this.wave-1)*0.12);
      bot.vx=length>0.1?tx/length*speed:0; bot.vz=length>0.1?tz/length*speed:0;
      moveHorizontal(this.world,bot,bot.vx*dt,bot.vz*dt,c.radius,c.height,0.34);
      moveVertical(this.world,bot,dt,c.radius,c.height,CONFIG.player.gravity);
      if (visible && targetDistance<36 && bot.shotTimer<=0 && this.projectiles.length<72) {
        this.projectiles.push({x:origin.x,y:origin.y,z:origin.z,
          vx:dir.x*c.projectileSpeed,vy:dir.y*c.projectileSpeed,vz:dir.z*c.projectileSpeed,life:4});
        bot.shotTimer=Math.max(0.8,c.shotInterval-this.wave*0.06)+Math.random()*0.5;
      }
      if (distance<1.2 && Math.abs(bot.y-p.y)<1.6 && bot.touchTimer===0) {
        this.hurt(c.damage); bot.touchTimer=0.8;
      }
    }
    // Écarter les robots sans moteur physique généraliste.
    for (let i=0;i<this.enemies.length;i++) {
      const a=this.enemies[i];
      if (a.health<=0) continue;
      for (let j=i+1;j<this.enemies.length;j++) {
        const b=this.enemies[j];
        if (b.health<=0) continue;
        let dx=a.x-b.x,dz=a.z-b.z;
        let distance=Math.hypot(dx,dz);
        if (distance>=1.12 || Math.abs(a.y-b.y)>1) continue;
        if (distance<0.001) {dx=1;dz=0;distance=1;}
        const push=Math.min(0.07,(1.12-distance)*0.5);
        moveHorizontal(this.world,a,dx/distance*push,dz/distance*push,c.radius,c.height);
        moveHorizontal(this.world,b,-dx/distance*push,-dz/distance*push,c.radius,c.height);
      }
    }
    this.enemies=this.enemies.filter(bot=>bot.health>0);
  }
  updateProjectiles(dt) {
    const p=this.player,c=CONFIG.player;
    const bounds={x0:p.x-c.radius,x1:p.x+c.radius,y0:p.y,y1:p.y+c.height,z0:p.z-c.radius,z1:p.z+c.radius};
    for (const bolt of this.projectiles) {
      bolt.life-=dt;
      const speed=Math.hypot(bolt.vx,bolt.vy,bolt.vz),length=speed*dt;
      const dir={x:bolt.vx/speed,y:bolt.vy/speed,z:bolt.vz/speed};
      const wall=this.world.ray(bolt,dir,length);
      const player=rayBox(bolt,dir,bounds,length);
      if (player<=wall && player<=length) {
        this.hurt(CONFIG.enemy.damage); bolt.life=0;
      } else if (wall<length) {
        this.burst(bolt.x+dir.x*wall,bolt.y+dir.y*wall,bolt.z+dir.z*wall,[1,0.4,0.2],2);
        bolt.life=0;
      } else {bolt.x+=bolt.vx*dt;bolt.y+=bolt.vy*dt;bolt.z+=bolt.vz*dt;}
    }
    this.projectiles=this.projectiles.filter(bolt=>bolt.life>0);
  }
  update(dt,input) {
    if (this.dead) return;
    this.time+=dt;
    this.messageTime=Math.max(0,this.messageTime-dt);
    this.hitTime=Math.max(0,this.hitTime-dt); this.damageTime=Math.max(0,this.damageTime-dt);
    this.recoil=Math.max(0,this.recoil-dt*11);
    this.shotCooldown=Math.max(0,this.shotCooldown-dt);
    updatePlayer(this.world,this.player,input,dt,kind=>this.audio.play(kind));
    if (input.firing && this.shotCooldown===0) this.shoot();
    this.navTimer-=dt;
    if (this.navTimer<=0) {
      this.world.navigation.update(this.player.x,this.player.z); this.navTimer=0.35;
    }
    this.updateEnemies(dt); this.updateProjectiles(dt);
    for (const pickup of this.pickups) {
      pickup.cooldown=Math.max(0,pickup.cooldown-dt);
      if (pickup.cooldown===0 && this.player.health<100 && this.player.y<1.2
        && Math.hypot(pickup.x-this.player.x,pickup.z-this.player.z)<1.3) {
        this.player.health=Math.min(100,this.player.health+30);
        pickup.cooldown=12; this.audio.play('pickup'); this.say('+30 INTÉGRITÉ',1);
      }
    }
    for (const particle of this.particles) {
      particle.life-=dt; particle.x+=particle.vx*dt; particle.y+=particle.vy*dt; particle.z+=particle.vz*dt;
      if (particle.vx || particle.vz) particle.vy-=9*dt;
    }
    this.particles=this.particles.filter(p=>p.life>0);
    if (this.dead) return;
    if (this.enemies.length===0) {
      if (this.waveTimer<0) {
        this.waveTimer=3;
        this.score+=250;
        this.player.health=Math.min(100,this.player.health+15);
        this.say('SECTEUR NETTOYÉ · +15 INTÉGRITÉ',2);
      }
      this.waveTimer-=dt;
      if (this.waveTimer<=0) this.startWave();
    }
  }
}
