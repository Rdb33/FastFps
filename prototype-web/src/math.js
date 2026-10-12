export const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
export const lerp = (a, b, t) => a + (b - a) * t;

export function perspective(out, fov, aspect, near = 0.05, far = 100) {
  out.fill(0);
  const f = 1 / Math.tan(fov / 2);
  out[0] = f / aspect; out[5] = f;
  out[10] = (far + near) / (near - far); out[11] = -1;
  out[14] = 2 * far * near / (near - far);
  return out;
}

export function viewMatrix(out, x, y, z, yaw, pitch) {
  const sy = Math.sin(yaw), cy = Math.cos(yaw);
  const sp = Math.sin(pitch), cp = Math.cos(pitch);
  const rx = cy, rz = sy, ux = -sy * sp, uy = cp, uz = cy * sp;
  const fx = sy * cp, fy = sp, fz = -cy * cp;
  out.set([rx, ux, -fx, 0, 0, uy, -fy, 0, rz, uz, -fz, 0,
    -(rx * x + rz * z), -(ux * x + uy * y + uz * z), fx * x + fy * y + fz * z, 1]);
  return out;
}

export function multiply(out, a, b) {
  for (let c = 0; c < 4; c++) {
    for (let r = 0; r < 4; r++) {
      out[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1]
        + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
    }
  }
  return out;
}

// Distance d'entrée dans une boîte. Le rayon doit être normalisé.
export function rayBox(origin, direction, box, maxDistance = Infinity) {
  let enter = 0, leave = maxDistance;
  for (const axis of ['x', 'y', 'z']) {
    const min = box[`${axis}0`], max = box[`${axis}1`];
    if (Math.abs(direction[axis]) < 1e-8) {
      if (origin[axis] < min || origin[axis] > max) return Infinity;
    } else {
      const a = (min - origin[axis]) / direction[axis];
      const b = (max - origin[axis]) / direction[axis];
      enter = Math.max(enter, Math.min(a, b));
      leave = Math.min(leave, Math.max(a, b));
      if (enter > leave) return Infinity;
    }
  }
  return enter <= maxDistance ? enter : Infinity;
}

export function direction(yaw, pitch) {
  const cp = Math.cos(pitch);
  return { x: Math.sin(yaw) * cp, y: Math.sin(pitch), z: -Math.cos(yaw) * cp };
}
