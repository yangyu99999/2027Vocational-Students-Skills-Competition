// Scan a large binary/asar file for byte patterns and print short context around hits.
import fs from 'node:fs';

const [, , file, ...pats] = process.argv;
const fd = fs.openSync(file, 'r');
const size = fs.fstatSync(fd).size;
const CHUNK = 8 * 1024 * 1024;
const OVERLAP = 4096;
const buf = Buffer.alloc(CHUNK + OVERLAP);
const patterns = pats.map((p) => ({ label: p, b: Buffer.from(p, 'latin1') }));
const hits = new Map(patterns.map((p) => [p.label, []]));
const seenCtx = new Set();
let pos = 0;
let carry = 0;
while (pos < size) {
  const want = Math.min(CHUNK, size - pos);
  const n = fs.readSync(fd, buf, carry, want, pos);
  const len = carry + n;
  for (const p of patterns) {
    let i = 0;
    const arr = hits.get(p.label);
    while (arr.length < 12) {
      const idx = buf.indexOf(p.b, i);
      if (idx === -1 || idx >= len) break;
      const abs = pos - carry + idx;
      // read a wide context window
      const cw = 400;
      const start = Math.max(0, abs - 160);
      const end = Math.min(size, abs + p.b.length + cw);
      const cbuf = Buffer.alloc(end - start);
      fs.readSync(fd, cbuf, 0, cbuf.length, start);
      const ctx = cbuf.toString('latin1').replace(/[\x00-\x08\x0b\x0c\x0e-\x1f]/g, '·');
      const key = p.label + '@' + abs;
      if (!seenCtx.has(key)) { seenCtx.add(key); arr.push({ off: abs, ctx }); }
      i = idx + 1;
    }
  }
  // keep overlap tail for next round
  const keep = Math.min(OVERLAP, len);
  buf.copy(buf, 0, len - keep, len);
  carry = keep;
  pos += n;
}
fs.closeSync(fd);
for (const p of patterns) {
  const arr = hits.get(p.label);
  console.log(`\n===== ${p.label} : ${arr.length} hits (capped 12) =====`);
  for (const h of arr) console.log(`--- @${h.off}\n${h.ctx}\n`);
}
console.log(`file=${file} size=${size}`);
