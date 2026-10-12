import { Mesh } from './geometry.js';
import { perspective, viewMatrix, multiply } from './math.js';
import { CONFIG } from './config.js';

const VERTEX = `
attribute vec3 aPosition;
attribute vec3 aColor;
attribute vec3 aNormal;
uniform mat4 uMatrix;
uniform float uFog;
varying vec3 vColor;
varying float vFog;
void main() {
  gl_Position = uMatrix * vec4(aPosition, 1.0);
  float light = 0.66 + 0.34 * max(dot(aNormal, normalize(vec3(-0.4, 0.9, 0.25))), 0.0);
  vColor = aColor * light;
  vFog = clamp((gl_Position.w - 16.0) / 65.0, 0.0, 0.75) * uFog;
}`;
const FRAGMENT = `
precision mediump float;
varying vec3 vColor;
varying float vFog;
void main() { gl_FragColor = vec4(mix(vColor, vec3(0.035, 0.065, 0.10), vFog), 1.0); }
`;

export class Renderer {
  constructor(canvas, world) {
    this.canvas = canvas;
    const gl = canvas.getContext('webgl', { antialias: false, depth: true,
      stencil: false, preserveDrawingBuffer: false, powerPreference: 'low-power' });
    if (!gl) throw new Error('WebGL est indisponible. Active l’accélération graphique du navigateur.');
    this.gl = gl;
    this.program = gl.createProgram();
    for (const [type, source] of [[gl.VERTEX_SHADER, VERTEX], [gl.FRAGMENT_SHADER, FRAGMENT]]) {
      const shader = gl.createShader(type);
      gl.shaderSource(shader, source); gl.compileShader(shader);
      if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(shader));
      gl.attachShader(this.program, shader); gl.deleteShader(shader);
    }
    gl.linkProgram(this.program);
    if (!gl.getProgramParameter(this.program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(this.program));
    gl.useProgram(this.program);
    this.attributes = ['aPosition','aColor','aNormal'].map(name => gl.getAttribLocation(this.program,name));
    this.uMatrix = gl.getUniformLocation(this.program,'uMatrix');
    this.uFog = gl.getUniformLocation(this.program,'uFog');
    this.worldBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER,this.worldBuffer);
    gl.bufferData(gl.ARRAY_BUFFER,world.mesh.array(),gl.STATIC_DRAW);
    this.worldVertices = world.mesh.vertices;
    this.dynamic = new Mesh(18000);
    this.dynamicBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER,this.dynamicBuffer);
    gl.bufferData(gl.ARRAY_BUFFER,this.dynamic.data.byteLength,gl.DYNAMIC_DRAW);
    this.gun = new Mesh(800);
    this.gunBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER,this.gunBuffer);
    gl.bufferData(gl.ARRAY_BUFFER,this.gun.data.byteLength,gl.DYNAMIC_DRAW);
    this.projection = new Float32Array(16); this.view = new Float32Array(16);
    this.matrix = new Float32Array(16);
    gl.enable(gl.DEPTH_TEST); gl.enable(gl.CULL_FACE);
    gl.clearColor(0.035,0.065,0.10,1);
    this.setPreset('pi');
  }
  setPreset(name) {
    this.preset = CONFIG.presets[name] || CONFIG.presets.pi;
    this.canvas.width = this.preset.width; this.canvas.height = this.preset.height;
    this.gl.viewport(0,0,this.canvas.width,this.canvas.height);
  }
  drawBuffer(buffer, count, matrix, fog = 1) {
    const gl = this.gl;
    gl.bindBuffer(gl.ARRAY_BUFFER,buffer);
    for (let i = 0; i < 3; i++) {
      gl.enableVertexAttribArray(this.attributes[i]);
      gl.vertexAttribPointer(this.attributes[i],3,gl.FLOAT,false,36,i*12);
    }
    gl.uniformMatrix4fv(this.uMatrix,false,matrix); gl.uniform1f(this.uFog,fog);
    gl.drawArrays(gl.TRIANGLES,0,count);
  }
  render(game, time) {
    const gl = this.gl, p = game.player;
    const speed = Math.hypot(p.vx,p.vz);
    const fov = (88 + Math.min(speed,24) * 0.35) * Math.PI / 180;
    perspective(this.projection,fov,this.canvas.width/this.canvas.height);
    const bob = p.grounded ? Math.sin(p.bob) * Math.min(speed/10,1) * 0.035 : 0;
    viewMatrix(this.view,p.x,p.y+CONFIG.player.eye+bob,p.z,p.yaw,p.pitch);
    multiply(this.matrix,this.projection,this.view);
    gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);
    this.drawBuffer(this.worldBuffer,this.worldVertices,this.matrix);
    const mesh = this.dynamic; mesh.reset();
    const metal = [0.30,0.39,0.46], red = [1,0.29,0.19], light = [1,0.64,0.26];
    for (const bot of game.enemies) {
      if (bot.health <= 0) continue;
      const color = bot.flash > 0 ? [1,0.92,0.78] : metal;
      mesh.box(bot.x,bot.y+0.35,bot.z,0.85,0.85,0.7,color);
      mesh.box(bot.x,bot.y+1.22,bot.z,0.65,0.42,0.6,red);
      mesh.box(bot.x,bot.y+1.34,bot.z,0.72,0.12,0.64,light);
      const stride = Math.sin(time*9+bot.id)*0.12;
      mesh.box(bot.x-0.27,bot.y,bot.z+stride,0.24,0.38,0.3,metal);
      mesh.box(bot.x+0.27,bot.y,bot.z-stride,0.24,0.38,0.3,metal);
      if (bot.health < 100) mesh.box(bot.x,bot.y+1.94,bot.z,0.8*bot.health/100,0.055,0.06,red);
    }
    for (const bolt of game.projectiles) mesh.diamond(bolt.x,bolt.y,bolt.z,0.16,red);
    for (const particle of game.particles) mesh.diamond(particle.x,particle.y,particle.z,particle.size*particle.life/particle.maxLife,particle.color);
    for (const pickup of game.pickups) {
      if (pickup.cooldown > 0) continue;
      const y = 0.5 + Math.sin(time*2+pickup.x)*0.12;
      mesh.box(pickup.x,y,pickup.z,0.24,0.65,0.24,[0.3,1,0.65]);
      mesh.box(pickup.x,y+0.2,pickup.z,0.65,0.24,0.24,[0.3,1,0.65]);
    }
    gl.bindBuffer(gl.ARRAY_BUFFER,this.dynamicBuffer);
    gl.bufferSubData(gl.ARRAY_BUFFER,0,mesh.array());
    this.drawBuffer(this.dynamicBuffer,mesh.vertices,this.matrix);
    // Arme en espace caméra : pas d'intersection avec les murs proches.
    this.gun.reset();
    const sway = Math.sin(p.bob)*Math.min(speed/10,1)*0.025;
    const recoil = game.recoil * 0.12;
    const gx = 0.30+sway, gy = -0.30-recoil, gz = -0.60+recoil;
    this.gun.box(gx,gy-0.14,gz,0.21,0.18,0.38,[0.20,0.29,0.36]);
    this.gun.box(gx,gy-0.01,gz-0.13,0.14,0.10,0.43,[0.41,0.52,0.57]);
    this.gun.box(gx,gy+0.02,gz-0.12,0.055,0.025,0.46,[0.26,0.95,0.81]);
    this.gun.box(gx,gy-0.28,gz+0.1,0.13,0.19,0.16,[0.14,0.20,0.26]);
    if (game.recoil > 0.7) this.gun.diamond(gx,gy+0.03,gz-0.4,0.095,[0.65,1,0.88]);
    gl.clear(gl.DEPTH_BUFFER_BIT);
    gl.bindBuffer(gl.ARRAY_BUFFER,this.gunBuffer);
    gl.bufferSubData(gl.ARRAY_BUFFER,0,this.gun.array());
    this.drawBuffer(this.gunBuffer,this.gun.vertices,this.projection,0);
  }
}
