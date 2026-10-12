import { CONFIG } from './config.js';

// Déplacement découpé pour que les dashes ne traversent pas les obstacles.
export function moveHorizontal(world, body, dx, dz, radius, height, stepHeight = 0) {
  const steps=Math.max(1,Math.ceil(Math.max(Math.abs(dx),Math.abs(dz))/(radius*0.6)));
  let hitX=false,hitZ=false;
  for (let i=0;i<steps;i++) {
    for (const [axis,delta,other] of [['x',dx/steps,'z'],['z',dz/steps,'x']]) {
      if (!delta) continue;
      let target=body[axis]+delta;
      for (const box of world.solids) {
        if (body.y>=box.y1-0.001 || body.y+height<=box.y0+0.001) continue;
        if (body[other]+radius<=box[`${other}0`] || body[other]-radius>=box[`${other}1`]) continue;
        if (target+radius<=box[`${axis}0`] || target-radius>=box[`${axis}1`]) continue;
        if (body.grounded && box.y1-body.y<=stepHeight+0.001
          && !world.blocked(axis==='x'?target:body.x,box.y1,axis==='z'?target:body.z,radius,height,box)) {
          body.y=box.y1;
          continue;
        }
        target=delta>0 ? box[`${axis}0`]-radius : box[`${axis}1`]+radius;
        if (axis==='x') hitX=true; else hitZ=true;
      }
      body[axis]=target;
    }
  }
  if (hitX) body.vx=0;
  if (hitZ) body.vz=0;
}

export function moveVertical(world,body,dt,radius,height,gravity) {
  const previous=body.y;
  body.vy-=gravity*dt;
  let target=body.y+body.vy*dt;
  body.grounded=false;
  if (target<=0) { target=0; body.vy=0; body.grounded=true; }
  for (const box of world.solids) {
    if (body.x+radius<=box.x0 || body.x-radius>=box.x1 || body.z+radius<=box.z0 || body.z-radius>=box.z1) continue;
    if (body.vy<=0 && previous>=box.y1-0.001 && target<=box.y1) {
      target=box.y1; body.vy=0; body.grounded=true;
    } else if (body.vy>0 && previous+height<=box.y0+0.001 && target+height>=box.y0) {
      target=box.y0-height; body.vy=0;
    }
  }
  body.y=target;
}

export function createPlayer() {
  return {x:0,y:0,z:16,vx:0,vy:0,vz:0,yaw:0,pitch:0,health:CONFIG.player.health,
    grounded:true,dashCooldown:0,dashTime:0,padCooldown:0,bob:0};
}

export function updatePlayer(world,p,input,dt,onSound) {
  const c=CONFIG.player;
  p.dashCooldown=Math.max(0,p.dashCooldown-dt);
  p.dashTime=Math.max(0,p.dashTime-dt);
  p.padCooldown=Math.max(0,p.padCooldown-dt);
  const sy=Math.sin(p.yaw),cy=Math.cos(p.yaw);
  let wx=input.right*cy+input.forward*sy;
  let wz=input.right*sy-input.forward*cy;
  const len=Math.hypot(wx,wz);
  if (len) { wx/=len; wz/=len; }
  // Maintenir espace permet d'enchaîner les sauts à l'atterrissage.
  if (p.grounded && input.jump) {
    p.vy=c.jump; p.grounded=false; onSound('jump');
  }
  if (input.dash && p.dashCooldown===0) {
    if (!len) { wx=sy; wz=-cy; }
    p.vx=wx*c.dashSpeed; p.vz=wz*c.dashSpeed;
    p.dashCooldown=c.dashCooldown; p.dashTime=c.dashDuration;
    onSound('dash');
  }
  if (p.dashTime===0) {
    if (p.grounded) {
      const friction=Math.max(0,1-c.friction*dt);
      p.vx*=friction; p.vz*=friction;
    }
    if (len) {
      const projected=p.vx*wx+p.vz*wz;
      const amount=Math.max(0,Math.min(c.speed-projected,(p.grounded?c.groundAcceleration:c.airAcceleration)*dt));
      p.vx+=wx*amount; p.vz+=wz*amount;
    }
    const speed=Math.hypot(p.vx,p.vz);
    if (speed>c.maxAirSpeed) { p.vx*=c.maxAirSpeed/speed; p.vz*=c.maxAirSpeed/speed; }
  }
  moveHorizontal(world,p,p.vx*dt,p.vz*dt,c.radius,c.height,c.stepHeight);
  moveVertical(world,p,dt,c.radius,c.height,c.gravity);
  for (const pad of world.pads) {
    if (p.grounded && p.padCooldown===0 && Math.abs(p.x-pad.x)<1.4 && Math.abs(p.z-pad.z)<1.4) {
      p.vy=15; p.grounded=false; p.padCooldown=0.5; onSound('jump');
    }
  }
  p.bob+=Math.hypot(p.vx,p.vz)*dt*1.6;
}
