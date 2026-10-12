// Position, couleur, normale : 9 nombres par sommet, sans texture.
export class Mesh {
  constructor(maxVertices = 0) {
    this.data = maxVertices ? new Float32Array(maxVertices * 9) : [];
    this.fixed = maxVertices > 0;
    this.length = 0;
  }
  reset() { this.length = 0; }
  vertex(x, y, z, color, nx, ny, nz) {
    if (this.fixed && this.length + 9 > this.data.length) return;
    const d = this.data;
    d[this.length++] = x; d[this.length++] = y; d[this.length++] = z;
    d[this.length++] = color[0]; d[this.length++] = color[1]; d[this.length++] = color[2];
    d[this.length++] = nx; d[this.length++] = ny; d[this.length++] = nz;
  }
  quad(a, b, c, d, color, normal) {
    for (const p of [a, b, c, a, c, d]) this.vertex(...p, color, ...normal);
  }
  box(x, y, z, w, h, depth, color) {
    const a = x - w / 2, b = x + w / 2, c = y, d = y + h;
    const e = z - depth / 2, f = z + depth / 2;
    this.quad([a,c,f],[b,c,f],[b,d,f],[a,d,f],color,[0,0,1]);
    this.quad([b,c,e],[a,c,e],[a,d,e],[b,d,e],color,[0,0,-1]);
    this.quad([b,c,f],[b,c,e],[b,d,e],[b,d,f],color,[1,0,0]);
    this.quad([a,c,e],[a,c,f],[a,d,f],[a,d,e],color,[-1,0,0]);
    this.quad([a,d,f],[b,d,f],[b,d,e],[a,d,e],color,[0,1,0]);
    this.quad([a,c,e],[b,c,e],[b,c,f],[a,c,f],color,[0,-1,0]);
  }
  // Octaèdre : huit triangles, pour projectiles et effets.
  diamond(x, y, z, r, color) {
    const points = [[x+r,y,z],[x,y,z+r],[x-r,y,z],[x,y,z-r]];
    for (let i = 0; i < 4; i++) {
      const a = points[i], b = points[(i+1)%4];
      this.quadTriangle([x,y+r,z],b,a,color,[0,1,0]);
      this.quadTriangle([x,y-r,z],a,b,color,[0,-1,0]);
    }
  }
  quadTriangle(a,b,c,color,n) { for (const p of [a,b,c]) this.vertex(...p,color,...n); }
  array() { return this.fixed ? this.data.subarray(0,this.length) : new Float32Array(this.data); }
  get vertices() { return this.length / 9; }
}
