import { Mesh } from './geometry.js';
import { rayBox } from './math.js';

export class World {
  constructor() {
    this.mesh = new Mesh();
    this.solids = [];
    this.spawns = [
      {x:-21,z:-17},{x:21,z:-17},{x:-21,z:17},{x:21,z:17},
      {x:0,z:-19},{x:23,z:0},{x:-23,z:0},{x:0,z:19},
    ];
    this.pads = [{x:-18,z:0},{x:18,z:0}];
    const m = this.mesh;
    for (let x = -28; x < 28; x += 4) {
      for (let z = -24; z < 24; z += 4) {
        const color = ((x+z)/4)%2 ? [0.12,0.18,0.22] : [0.14,0.21,0.25];
        m.quad([x,0,z+4],[x+4,0,z+4],[x+4,0,z],[x,0,z],color,[0,1,0]);
      }
    }
    this.solid(0,0,-24,56,7,1,[0.15,0.24,0.30]);
    this.solid(0,0,24,56,7,1,[0.15,0.24,0.30]);
    this.solid(-28,0,0,1,7,48,[0.13,0.21,0.27]);
    this.solid(28,0,0,1,7,48,[0.13,0.21,0.27]);
    const cyan = [0.24,0.82,0.73], orange = [0.95,0.42,0.2];
    for (const z of [-23.45,23.45]) {
      m.box(0,1.1,z,55,0.1,0.04,cyan);
      m.box(0,5,z,55,0.15,0.04,cyan);
      for (let x=-24;x<=24;x+=8) m.box(x,0,z,0.14,7,0.05,[0.24,0.35,0.40]);
    }
    for (const x of [-27.45,27.45]) {
      m.box(x,1.1,0,0.04,0.1,47,cyan);
      m.box(x,5,0,0.04,0.15,47,cyan);
    }
    for (const x of [-10,10]) {
      for (const z of [-8,8]) {
        this.solid(x,0,z,3.2,4.2,3.2,[0.22,0.32,0.39]);
        m.box(x,4.2,z,3.35,0.1,3.35,cyan);
        m.box(x,0.8,z,3.28,0.14,3.28,[0.75,0.34,0.17]);
      }
    }
    this.solid(0,0,0,9,1.8,7,[0.25,0.32,0.39]);
    m.box(0,1.8,0,9.15,0.04,7.15,cyan);
    // Six marches de 30 cm pour accéder à la plateforme centrale.
    for (let i=0;i<6;i++) {
      this.solid(0,0,6.25-i*0.5,3,0.3*(i+1),0.5,[0.22+i*0.012,0.3+i*0.01,0.34+i*0.01]);
      m.box(0,0.3*(i+1)+0.005,6.46-i*0.5,3,0.018,0.04,cyan);
    }
    for (const x of [-19,19]) {
      for (const z of [-12,12]) this.solid(x,0,z,3,1.15,2.5,[0.32,0.37,0.41]);
    }
    for (const pad of this.pads) {
      m.box(pad.x,0.005,pad.z,2.8,0.045,2.8,[0.12,0.32,0.38]);
      m.box(pad.x,0.055,pad.z,2,0.025,2,orange);
      m.box(pad.x,0.085,pad.z,1.1,0.025,1.1,[1,0.7,0.32]);
    }
    // Bandes au sol, sans textures ni transparence.
    for (const x of [-15,15]) m.box(x,0.003,0,0.055,0.006,41,[0.28,0.47,0.48]);
    for (const z of [-17,17]) m.box(0,0.003,z,45,0.006,0.055,[0.28,0.47,0.48]);
    this.navigation = new Navigation(this);
  }
  solid(x,y,z,w,h,d,color) {
    this.solids.push({x0:x-w/2,x1:x+w/2,y0:y,y1:y+h,z0:z-d/2,z1:z+d/2});
    this.mesh.box(x,y,z,w,h,d,color);
  }
  blocked(x,y,z,radius,height,ignore = null) {
    return this.solids.some(b => b !== ignore && y < b.y1-0.001 && y+height > b.y0+0.001
      && x+radius > b.x0 && x-radius < b.x1 && z+radius > b.z0 && z-radius < b.z1);
  }
  ray(origin,direction,range) {
    let nearest = range;
    for (const box of this.solids) nearest = Math.min(nearest,rayBox(origin,direction,box,nearest));
    // Sol de l'arène.
    if (direction.y < -0.00001) {
      const ground = -origin.y/direction.y;
      if (ground >= 0) nearest = Math.min(nearest,ground);
    }
    return nearest;
  }
}

// Un champ de distances partagé par tous les robots, recalculé à 3 Hz.
class Navigation {
  constructor(world) {
    this.width = 28; this.height = 24; this.cell = 2;
    this.passable = new Uint8Array(this.width*this.height);
    this.distance = new Int16Array(this.passable.length);
    this.queue = new Int16Array(this.passable.length);
    for (let i=0;i<this.passable.length;i++) {
      const p = this.center(i);
      this.passable[i] = world.blocked(p.x,0.35,p.z,0.8,1.6) ? 0 : 1;
    }
    this.distance.fill(-1);
  }
  index(x,z) {
    const cx = Math.max(0,Math.min(this.width-1,Math.floor((x+28)/this.cell)));
    const cz = Math.max(0,Math.min(this.height-1,Math.floor((z+24)/this.cell)));
    return cz*this.width+cx;
  }
  center(i) { return {x:(i%this.width)*this.cell-27,z:Math.floor(i/this.width)*this.cell-23}; }
  neighbours(i) {
    const out = [], x = i%this.width;
    if (x>0) out.push(i-1);
    if (x<this.width-1) out.push(i+1);
    if (i>=this.width) out.push(i-this.width);
    if (i<this.passable.length-this.width) out.push(i+this.width);
    return out;
  }
  update(x,z) {
    let target = this.index(x,z);
    if (!this.passable[target]) {
      let best = Infinity;
      for (let i=0;i<this.passable.length;i++) {
        if (!this.passable[i]) continue;
        const p = this.center(i), d = (p.x-x)**2+(p.z-z)**2;
        if (d<best) { best=d; target=i; }
      }
    }
    this.distance.fill(-1);
    this.distance[target]=0;
    let head=0,tail=0;
    this.queue[tail++]=target;
    while (head<tail) {
      const current=this.queue[head++];
      for (const next of this.neighbours(current)) {
        if (this.passable[next] && this.distance[next]===-1) {
          this.distance[next]=this.distance[current]+1;
          this.queue[tail++]=next;
        }
      }
    }
  }
  next(x,z) {
    const current=this.index(x,z);
    let best=current, distance=this.distance[current];
    for (const next of this.neighbours(current)) {
      if (this.distance[next]>=0 && (distance<0 || this.distance[next]<distance)) {
        best=next; distance=this.distance[next];
      }
    }
    return this.center(best);
  }
}
