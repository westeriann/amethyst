// Generates src/app.ico: a glowing amethyst-toned ring (no dependencies).
const fs = require('fs');
const stops = [[0, [155,93,229]], [0.33, [224,170,255]], [0.66, [123,44,191]], [1, [155,93,229]]];  // amethyst
const grad = (t) => { for (let i = 0; i < stops.length - 1; i++) { const [t0,c0] = stops[i], [t1,c1] = stops[i+1]; if (t <= t1) { const k = (t-t0)/(t1-t0); return c0.map((a,j) => Math.round(a + (c1[j]-a)*k)); } } return stops[3][1]; };
function image(n) {
  const px = Buffer.alloc(n * n * 4); let o = 0;
  for (let y = n - 1; y >= 0; y--) for (let x = 0; x < n; x++) {
    const dx = (x + .5)/n - .5, dy = (y + .5)/n - .5, r = Math.hypot(dx, dy);
    const ang = ((Math.atan2(dy, dx) / (2*Math.PI)) % 1 + 1) % 1;
    const ring = Math.max(0, 1 - Math.abs(r - 0.33)/0.11), glow = Math.max(0, 1 - Math.abs(r - 0.33)/0.17)*0.5, core = Math.max(0, 1 - r/0.2)*0.35;
    const a = Math.min(1, ring + glow*0.6 + core), c = grad(ang), b = Math.min(1, ring*1.1 + 0.15);
    px[o++] = c[2]*b; px[o++] = c[1]*b; px[o++] = c[0]*b; px[o++] = 255*a;
  }
  const hdr = Buffer.alloc(40); hdr.writeUInt32LE(40,0); hdr.writeInt32LE(n,4); hdr.writeInt32LE(n*2,8); hdr.writeUInt16LE(1,12); hdr.writeUInt16LE(32,14); hdr.writeUInt32LE(px.length,20);
  return Buffer.concat([hdr, px, Buffer.alloc(Math.ceil(n/32)*4*n)]);
}
const sizes = [16, 24, 32, 48, 64, 128, 256], imgs = sizes.map(image);
const head = Buffer.alloc(6 + 16*sizes.length); head.writeUInt16LE(1,2); head.writeUInt16LE(sizes.length,4);
let off = head.length;
sizes.forEach((s,i) => { const e = 6 + 16*i; head[e] = s % 256; head[e+1] = s % 256; head.writeUInt16LE(1,e+4); head.writeUInt16LE(32,e+6); head.writeUInt32LE(imgs[i].length,e+8); head.writeUInt32LE(off,e+12); off += imgs[i].length; });
fs.writeFileSync(process.argv[2], Buffer.concat([head, ...imgs]));
