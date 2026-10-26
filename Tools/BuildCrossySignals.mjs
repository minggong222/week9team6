import { readFile, writeFile, unlink } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

// Kenney City Kit Roads 2.2 (CC0): preserve the pole/housing and darken unused lenses.
const folder = fileURLToPath(new URL('../Assets/Models/Crossy/Signals/', import.meta.url));
const source = (await readFile(folder + 'traffic-light.obj', 'utf8')).split(/\r?\n/);
const vertices = source.filter(l => l.startsWith('v ')).map(l => l.split(/\s+/).slice(1, 4).map(Number));
const uvs = source.filter(l => l.startsWith('vt ')).map(l => l.split(/\s+/).slice(1, 3).map(Number));
const min = [0, 1, 2].map(a => Math.min(...vertices.map(v => v[a])));
const max = [0, 1, 2].map(a => Math.max(...vertices.map(v => v[a])));
const scale = 2.4 / (max[1] - min[1]);
const center = max.map((v, a) => (v + min[a]) / 2);
const redPoints = [];
for (const on of [false, true]) {
    const output = ['# Adapted from Kenney City Kit Roads (CC0); see License.txt.', 'mtllib crossy-rail-signal.mtl'];
    let material;
    for (const line of source) {
        if (line.startsWith('v ')) {
            const v = line.split(/\s+/).slice(1, 4).map(Number);
            output.push('v ' + v.map((n, a) => ((n - (a === 1 ? min[a] : center[a])) * scale).toFixed(7)).join(' '));
        } else if (/^(vt|vn) /.test(line)) output.push(line);
        else if (line.startsWith('f ')) {
            const corners = line.slice(2).split(/\s+/).map(c => c.split('/').map(Number));
            const isPaletteColumn = u => corners.every(c => Math.abs(uvs[c[1] - 1][0] - u) < 0.00001);
            const red = isPaletteColumn(0.59375);
            const dark = isPaletteColumn(0.96875) || isPaletteColumn(0.71875);
            const next = red ? (on ? 'RedOn' : 'RedOff') : dark ? 'DarkLens' : 'colormap';
            if (!on && red) for (const c of corners) redPoints.push(vertices[c[0] - 1]);
            if (next !== material) { output.push('usemtl ' + next); material = next; }
            output.push(line);
        }
    }
    const path = folder + 'crossy-rail-signal-' + (on ? 'on' : 'off') + '.obj';
    await writeFile(path, output.join('\n') + '\n');
    try { await unlink(path + '.bin'); } catch (e) { if (e.code !== 'ENOENT') throw e; }
}
await writeFile(folder + 'crossy-rail-signal.mtl', `newmtl colormap
Kd 1 1 1
map_Kd Textures/colormap.png

newmtl RedOn
Kd 1 0.035 0.015

newmtl RedOff
Kd 0.07 0.012 0.012

newmtl DarkLens
Kd 0.035 0.035 0.04
`);
const redCenter = [0, 1, 2].map(a => (Math.min(...redPoints.map(v => v[a])) + Math.max(...redPoints.map(v => v[a]))) / 2);
console.log(JSON.stringify({ height: 2.4, redLensEngine: [-(redCenter[2] - center[2]) * scale,
    (redCenter[0] - center[0]) * scale, (redCenter[1] - min[1]) * scale], source: 'https://kenney.nl/assets/city-kit-roads' }));
