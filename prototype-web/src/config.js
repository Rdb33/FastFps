// Tous les réglages de gameplay et les budgets de rendu sont centralisés ici.
export const CONFIG = Object.freeze({
  fixedStep: 1 / 60,
  maxSteps: 6,
  player: { radius: 0.38, height: 1.7, eye: 1.48, health: 100,
    speed: 10, maxAirSpeed: 16, groundAcceleration: 65, airAcceleration: 17,
    friction: 8, gravity: 25, jump: 9, stepHeight: 0.34,
    dashSpeed: 24, dashDuration: 0.16, dashCooldown: 1.4 },
  weapon: { damage: 34, interval: 0.15, range: 85 },
  enemy: { radius: 0.55, height: 1.65, health: 100, speed: 3.8,
    shotInterval: 2.3, projectileSpeed: 13, damage: 12, maxAlive: 12 },
  presets: {
    pi: { width: 640, height: 360, fps: 30, label: 'PI 4', particles: 28 },
    balanced: { width: 960, height: 540, fps: 60, label: 'ÉQUILIBRÉ', particles: 64 },
    desktop: { width: 1280, height: 720, fps: 60, label: 'PC', particles: 100 },
  },
});
