import { readFile, writeFile, unlink } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

// Source OBJ/MTL and the palette are copied unchanged from Kenney Train Kit (CC0).
const root = fileURLToPath(new URL('../', import.meta.url));
const folder = root + 'Assets/Models/Crossy/Trains/';
const engine = ([x, y, z]) => [-z, x, y];
const obj = ([x, y, z]) => [y, z, -x];
async function readMesh(path) {
    const mesh = { v: [], vt: [], vn: [], faces: [] };
    let material;
    for (const line of (await readFile(path, 'utf8')).split(/\r?\n/)) {
        const [key, ...values] = line.trim().split(/\s+/);
        if (['v', 'vt', 'vn'].includes(key)) mesh[key].push(values.map(Number));
        if (key === 'usemtl') material = values[0];
        if (key === 'f') mesh.faces.push({ material, indices: values.map(v => v.split('/').map(i => i ? Number(i) : null)) });
    }
    const points = mesh.v.map(engine);
    mesh.min = [0, 1, 2].map(i => Math.min(...points.map(p => p[i])));
    mesh.max = [0, 1, 2].map(i => Math.max(...points.map(p => p[i])));
    mesh.center = mesh.max.map((v, i) => (v + mesh.min[i]) / 2);
    return mesh;
}
const grass = await readMesh(root + 'Assets/Models/Crossy/RoadTiles/kenney-grass-row.obj');
const rail = await readMesh(folder + 'railroad-straight.obj');
const head = await readMesh(folder + 'train-electric-bullet-a.obj');
const carriage = await readMesh(folder + 'train-electric-bullet-b.obj');
const cityHead = await readMesh(folder + 'train-electric-city-a.obj');
const cityCarriage = await readMesh(folder + 'train-electric-city-b.obj');
const diesel = await readMesh(folder + 'train-diesel-a.obj');
const box = await readMesh(folder + 'train-carriage-box.obj');
const container = await readMesh(folder + 'train-carriage-container-blue.obj');
const tank = await readMesh(folder + 'train-carriage-tank.obj');

async function compose(name, pieces) {
    const vertices = [], uvs = [], normals = [], groups = new Map();
    for (const { mesh, scale = [1, 1, 1], yaw = 0, offset = [0, 0, 0], center = false } of pieces) {
        const counts = [vertices.length, uvs.length, normals.length];
        const a = yaw * Math.PI / 180, c = Math.cos(a), s = Math.sin(a);
        const rotate = ([x, y, z]) => [x * c - y * s, x * s + y * c, z];
        for (const point of mesh.v) {
            const p = engine(point).map((v, i) => (v - (center ? (i === 2 ? mesh.min[i] : mesh.center[i]) : 0)) * scale[i]);
            vertices.push(obj(rotate(p).map((v, i) => v + offset[i])));
        }
        uvs.push(...mesh.vt);
        for (const normal of mesh.vn) {
            const n = rotate(engine(normal).map((v, i) => v / scale[i]));
            const length = Math.hypot(...n);
            normals.push(obj(n.map(v => v / length)));
        }
        for (const face of mesh.faces) {
            if (!groups.has(face.material)) groups.set(face.material, []);
            groups.get(face.material).push('f ' + face.indices.map(corner => corner.map((i, axis) => i === null ? '' : i + counts[axis]).join('/')).join(' '));
        }
    }
    const lines = ['# Assembled from Kenney Train Kit; see License.txt.', 'mtllib crossy-train.mtl'];
    for (const [key, values] of [['v', vertices], ['vt', uvs], ['vn', normals]])
        for (const value of values) lines.push(key + ' ' + value.map(v => Number(v.toFixed(7))).join(' '));
    for (const [material, faces] of groups) lines.push('usemtl ' + material, ...faces);
    await writeFile(folder + name + '.obj', lines.join('\n') + '\n');
    // Engine binary caches do not check source timestamps.
    try { await unlink(folder + name + '.obj.bin'); } catch (e) { if (e.code !== 'ENOENT') throw e; }
    return vertices.map(engine);
}
await writeFile(folder + 'crossy-train.mtl',
    'newmtl colormap\nKd 1 1 1\nmap_Kd Textures/colormap.png\n\n' +
    'newmtl Grass\nKd 1 1 1\nmap_Kd ../RoadTiles/kenney-color-Grass.png\n');

// One 3 x 69 row. Rail bed sits on the same grass floor used by the map.
await compose('crossy-rail-row', [{ mesh: grass }, ...Array.from({ length: 23 }, (_, i) => ({
    mesh: rail, scale: [0.75, 2.2, 2.2], yaw: 90, center: true, offset: [0, (i - 11) * 3, 0.01]
}))]);

async function buildTrain(name, meshes, hasRearCab) {
    // Scale the complete formation uniformly; keep every original carriage's proportions.
    const scale = 2 / Math.max(...meshes.map(m => m.max[1] - m.min[1]));
    const gap = 0.15, lengths = meshes.map(m => (m.max[0] - m.min[0]) * scale);
    const length = lengths.reduce((a, b) => a + b, 0) + gap * (meshes.length - 1);
    let cursor = length / 2;
    await compose(name, meshes.map((mesh, i) => {
        const y = cursor - lengths[i] / 2; cursor -= lengths[i] + gap;
        // Kenney cab noses face OBJ +Z. Front faces engine +Y; rear faces -Y.
        const yaw = hasRearCab && i === meshes.length - 1 ? 90 : -90;
        return { mesh, scale: [scale, scale, scale], yaw, center: true, offset: [0, y, 0.23] };
    }));
    return { name, width: 2, length, cars: meshes.length };
}
const variants = [
    await buildTrain('crossy-train-long', [head, carriage, carriage, carriage, head], true),
    await buildTrain('crossy-train-city', [cityHead, cityCarriage, cityCarriage, cityCarriage, cityHead], true),
    await buildTrain('crossy-train-freight', [diesel, box, container, tank, box], false)
];
console.log(JSON.stringify({ railRow: [3, 69], variants }));
