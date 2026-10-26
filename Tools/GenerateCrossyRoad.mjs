import { readFile, writeFile, unlink } from 'node:fs/promises';
import { deflateSync } from 'node:zlib';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const folder = root + 'Assets/Models/Crossy/RoadTiles/';
const grid = 3, tileScale = grid / 3;
const rowCount = 40, playerLength = 2, vehicleWidth = 2;
const mapCenterX = (rowCount - 1) * grid / 2;
const roadStarts = [3, 8, 13, 18, 23, 28, 33];
const roadWidths = [2, 3, 2, 3, 2, 2, 3];
const sources = new Map(), materials = new Map();

// Keep Kenney's original faces, UVs, normals and material colors.
for (const id of ['002', '019', '020', '034', '059', '163']) {
    const mesh = { v: [], vt: [], vn: [], faces: [] };
    let material = '';
    for (const line of (await readFile(folder + 'roadTile_' + id + '.obj', 'utf8')).split(/\r?\n/)) {
        const [key, ...values] = line.trim().split(/\s+/);
        if (['v', 'vt', 'vn'].includes(key)) mesh[key].push(values.map(Number));
        if (key === 'usemtl') material = values[0];
        if (key === 'f') mesh.faces.push({ material, indices: values.map(value => value.split('/').map(index => index ? Number(index) : null)) });
    }
    sources.set(id, mesh);
    let name = '';
    for (const line of (await readFile(folder + 'roadTile_' + id + '.mtl', 'utf8')).split(/\r?\n/)) {
        const [key, ...values] = line.trim().split(/\s+/);
        if (key === 'newmtl') { name = values[0]; if (!materials.has(name)) materials.set(name, []); }
        if (['Ka', 'Kd', 'Ks', 'Ns', 'd'].includes(key)) {
            const record = materials.get(name);
            if (!record.some(item => item.startsWith(key + ' '))) record.push(key + ' ' + values.join(' '));
        }
    }
}
// Route Kenney's color swatches through the same texture decoding as car colormaps.
function crc32(bytes) {
    let crc = 0xffffffff;
    for (const byte of bytes) {
        crc ^= byte;
        for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
    return (crc ^ 0xffffffff) >>> 0;
}
function pngChunk(type, bytes) {
    const name = Buffer.from(type), size = Buffer.alloc(4), crc = Buffer.alloc(4);
    size.writeUInt32BE(bytes.length);
    crc.writeUInt32BE(crc32(Buffer.concat([name, bytes])));
    return Buffer.concat([size, name, bytes, crc]);
}
async function writeSwatch(filename, color) {
    const size = 4, bytes = Buffer.alloc(size * (1 + size * 3));
    for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) for (let channel = 0; channel < 3; channel++)
        bytes[y * (1 + size * 3) + 1 + x * 3 + channel] = Math.round(Math.max(0, Math.min(1, color[channel])) * 255);
    const header = Buffer.alloc(13);
    header.writeUInt32BE(size, 0); header.writeUInt32BE(size, 4); header[8] = 8; header[9] = 2;
    await writeFile(folder + filename, Buffer.concat([
        Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), pngChunk('IHDR', header),
        pngChunk('IDAT', deflateSync(bytes)), pngChunk('IEND', Buffer.alloc(0))
    ]));
}
async function clearMeshCache(name) {
    // This engine reads OBJ.bin without checking OBJ/MTL modification times.
    try { await unlink(folder + name + '.obj.bin'); } catch (error) { if (error.code !== 'ENOENT') throw error; }
}
const texturedMaterials = ['# Generated color swatches from original Kenney Kd values; see License.txt.'];
for (const [name, lines] of materials) {
    const color = lines.find(line => line.startsWith('Kd ')).split(/\s+/).slice(1).map(Number);
    const filename = 'kenney-color-' + name + '.png';
    await writeSwatch(filename, color);
    texturedMaterials.push('newmtl ' + name, ...lines.filter(line => !line.startsWith('Kd ')),
        'Kd 1 1 1', 'map_Kd ' + filename, '');
}
await writeFile(folder + 'kenney-map.mtl', texturedMaterials.join('\n') + '\n');
for (const id of ['019', '020']) {
    const name = 'kenney-tree-' + id;
    const original = await readFile(folder + 'roadTile_' + id + '.obj', 'utf8');
    await writeFile(folder + name + '.obj', original.replace(/^mtllib\s+.*$/m, 'mtllib kenney-map.mtl'));
    await clearMeshCache(name);
}
const engine = ([x, y, z]) => [-z, x, y];
const obj = ([x, y, z]) => [y, z, -x];
const rotate = ([x, y, z], yaw) => {
    const a = yaw * Math.PI / 180;
    return [x * Math.cos(a) - y * Math.sin(a), x * Math.sin(a) + y * Math.cos(a), z];
};
const number = n => String(Math.abs(n) < 1e-9 ? 0 : Number(n.toFixed(8)));
const tile = (id, x = 0, y = 0, yaw = 0, top = 0.55, sx = tileScale, sy = tileScale, z = 0) =>
    ({ id, x, y, yaw, top, scale: [sx, sy, tileScale], z });
async function merge(name, placements) {
    const v = [], vt = [], vn = [], groups = new Map();
    for (const p of placements) {
        const mesh = sources.get(p.id), offsets = [v.length, vt.length, vn.length];
        const [sx, sy, sz] = p.scale;
        for (const point of mesh.v) {
            const e = engine(point);
            const q = rotate([(e[0] - 1.5) * sx, (e[1] - 1.5) * sy, (e[2] - p.top) * sz], p.yaw);
            v.push(obj([q[0] + p.x, q[1] + p.y, q[2] + p.z]));
        }
        vt.push(...mesh.vt);
        for (const normal of mesh.vn) {
            const e = engine(normal), q = rotate([e[0] / sx, e[1] / sy, e[2] / sz], p.yaw);
            const length = Math.hypot(...q) || 1;
            vn.push(obj(q.map(n => n / length)));
        }
        const counts = [mesh.v.length, mesh.vt.length, mesh.vn.length];
        for (const face of mesh.faces) {
            if (!groups.has(face.material)) groups.set(face.material, []);
            const faceIndices = face.indices.map(index => index.map((n, channel) =>
                n === null ? '' : (n > 0 ? n : counts[channel] + n + 1) + offsets[channel]).join('/'));
            groups.get(face.material).push('f ' + faceIndices.join(' '));
        }
    }
    const lines = ['# Assembled from original Kenney tiles; see License.txt.', 'mtllib kenney-map.mtl'];
    for (const [key, values] of [['v', v], ['vt', vt], ['vn', vn]])
        for (const value of values) lines.push(key + ' ' + value.map(number).join(' '));
    for (const [material, faces] of groups) lines.push('usemtl ' + material, ...faces);
    await writeFile(folder + name + '.obj', lines.join('\n') + '\n');
    await clearMeshCache(name);
}
const columns = Array.from({ length: 23 }, (_, i) => i - 11);
await merge('kenney-grass-row', columns.map(c => tile('163', 0, c * grid)));
await merge('kenney-plaza-row', columns.map(c => Math.abs(c) <= 3 ? tile('002', 0, c * grid, 0, 0.63) : tile('163', 0, c * grid)));
await merge('kenney-sidewalk', Array.from({ length: rowCount }, (_, r) => tile('002', r * grid - mapCenterX, 0, 0, 0.63)));
await merge('kenney-backdrop', [tile('163', 0, 0, 0, 0.55, (rowCount + 4) * grid / 3, 32 * grid / 3, -0.08)]);
// Two half-width edge tiles form one lane, with white lines on its outside edges.
await merge('kenney-road-row', columns.flatMap(c => [
    tile('059', -grid / 4, c * grid, -90, 0.6, tileScale, tileScale / 2),
    tile('059', grid / 4, c * grid, 90, 0.6, tileScale, tileScale / 2)
]));
const transform = (x = 0, y = 0, z = 0, yaw = 0, scale = 1) => ({
    Location: { X: x, Y: y, Z: z }, Rotation: { Pitch: 0, Roll: 0, Yaw: yaw }, Scale: { X: scale, Y: scale, Z: scale }
});
const component = (Class, Name, Parent, Transform, extra = {}) => ({
    Class, Name, Parent, Properties: { Transform, bAutoActivate: true, bHiddenInGame: false, bVisible: true, ...extra }
});
function actor(name, mesh, x, y, yaw = 0, local = transform()) {
    return { Class: 'AStaticMeshActor', Name: name, Properties: { bActorEnableCollision: false }, Components: [
        component('USceneComponent', 'DefaultSceneRoot', '', transform(x, y, 0, yaw)),
        { ...component('UStaticMeshComponent', 'UStaticMeshComponent', 'DefaultSceneRoot', local,
            { StaticMesh: 'Models/Crossy/RoadTiles/' + mesh + '.obj' }), OverrideMaterials: [] }
    ] };
}
if (!process.argv.includes('--assets-only')) {
const scenePath = root + 'Scenes/Crossy.scene';
const scene = JSON.parse(await readFile(scenePath, 'utf8'));
const previousActors = scene.World.PersistentLevel.Actors;
const rootTransform = a => a.Components.find(c => c.Name === 'DefaultSceneRoot').Properties.Transform;
// The editor normalizes numeric FName suffixes (Grass_01 becomes Grass_1).
const referenceGrassRow = previousActors.find(a => /^Grass_0*1$/.test(a.Name));
if (!referenceGrassRow) throw new Error('Grass row 1 is needed to identify the previous grid.');
const previousGrid = rootTransform(referenceGrassRow).Location.X;
if (!(previousGrid > 0)) throw new Error('Invalid previous grid size.');
const carTemplates = previousActors.filter(a => /^Car_L\d+_\d+$/.test(a.Name));
if (!carTemplates.length) throw new Error('No car templates found in Crossy.scene.');
// Preserve Player, cameras and unrelated editor additions. Rebuild generated lanes and cars.
const actors = previousActors.filter(a =>
    !/^(Car_L\d+_\d+$|Ground_|Grass_|Road_|LaneMark_|Tree_|Boundary_|Start_Line$|Finish_Tile_)/.test(a.Name));
// Move existing roadside decorations to the new grid; repeated generation keeps their placement stable.
for (const decoration of actors.filter(a => /^(Rock_|Bench_|Cone_)/.test(a.Name))) {
    const location = rootTransform(decoration).Location;
    location.X *= grid / previousGrid;
    location.Y *= grid / previousGrid;
}
const boundsCache = new Map();
async function meshBounds(path) {
    if (boundsCache.has(path)) return boundsCache.get(path);
    const vertices = (await readFile(root + 'Assets/' + path, 'utf8')).split(/\r?\n/)
        .filter(line => /^v\s/.test(line)).map(line => engine(line.trim().split(/\s+/).slice(1, 4).map(Number)));
    if (!vertices.length) throw new Error('No vertices in ' + path);
    const min = [0, 1, 2].map(axis => Math.min(...vertices.map(v => v[axis])));
    const max = [0, 1, 2].map(axis => Math.max(...vertices.map(v => v[axis])));
    const bounds = { min, max, size: max.map((value, axis) => value - min[axis]),
        center: max.map((value, axis) => (value + min[axis]) / 2) };
    boundsCache.set(path, bounds);
    return bounds;
}
const player = actors.find(a => a.Name === 'Player');
const playerMesh = player?.Components.find(c => c.Properties.StaticMesh === 'Models/OBJ format/animal-pig.obj');
if (!playerMesh) throw new Error('Player pig mesh not found.');
const pigBounds = await meshBounds(playerMesh.Properties.StaticMesh);
const pigScale = playerLength / pigBounds.size[0];
playerMesh.Properties.Transform.Scale = { X: pigScale, Y: pigScale, Z: pigScale };
const pigCenter = rotate(pigBounds.center.map(value => value * pigScale), playerMesh.Properties.Transform.Rotation.Yaw);
playerMesh.Properties.Transform.Location = { X: -pigCenter[0], Y: -pigCenter[1], Z: -pigBounds.min[2] * pigScale };
const roadRows = new Set(roadStarts.flatMap((start, i) => Array.from({ length: roadWidths[i] }, (_, r) => start + r)));
actors.push(actor('Ground_Backdrop', 'kenney-backdrop', mapCenterX, 0));
for (let row = 0; row < rowCount; row++) {
    if (roadRows.has(row)) continue;
    actors.push(actor('Grass_' + String(row).padStart(2, '0'), row < 3 || row >= 37 ? 'kenney-plaza-row' : 'kenney-grass-row', row * grid, 0));
    for (const side of [-1, 1]) {
        const id = row % 2 ? '019' : '020', positions = sources.get(id).v.map(engine);
        const center = [0, 1].map(axis => (Math.min(...positions.map(p => p[axis])) + Math.max(...positions.map(p => p[axis]))) / 2);
        const scale = 1.4 + row % 3 * 0.15;
        actors.push(actor('Tree_' + row + '_' + (side < 0 ? 'L' : 'R'), 'kenney-tree-' + id,
            row * grid, side * (10.4 + row % 3 * 0.2) * grid, row * 37 % 360,
            transform(-center[0] * scale, -center[1] * scale, 0, 0, scale)));
    }
}
for (const side of [-1, 1]) actors.push(actor(side < 0 ? 'Boundary_Left' : 'Boundary_Right', 'kenney-sidewalk', mapCenterX, side * 9 * grid));
for (const [lane, row] of [...roadRows].entries()) {
    actors.push(actor('Road_Row_' + String(row).padStart(2, '0'), 'kenney-road-row', row * grid, 0));
    const car = structuredClone(carTemplates[lane % carTemplates.length]);
    car.Name = 'Car_L' + (lane + 1) + '_1';
    const carRoot = rootTransform(car);
    carRoot.Location.X = row * grid;
    carRoot.Location.Y = (lane % 3 - 1) * 4 * grid;
    carRoot.Rotation.Yaw = lane % 2 === 0 ? 90 : -90;
    carRoot.Scale = { X: 1, Y: 1, Z: 1 };
    const carMesh = car.Components.find(c => c.Properties.StaticMesh);
    const bounds = await meshBounds(carMesh.Properties.StaticMesh);
    // Cars face along Y, so their local Y width occupies the lane's X width.
    const scale = vehicleWidth / bounds.size[1];
    carMesh.Properties.Transform = transform(-bounds.center[0] * scale, -bounds.center[1] * scale,
        -bounds.min[2] * scale, 0, scale);
    actors.push(car);
}
scene.World.PersistentLevel.Actors = actors;
const playerScriptPath = root + 'Assets/Scripts/Player.lua';
const playerScript = await readFile(playerScriptPath, 'utf8');
if (!/^local MoveDistance = [\d.]+/m.test(playerScript)) throw new Error('Player MoveDistance not found.');
await writeFile(playerScriptPath, playerScript.replace(/^local MoveDistance = [\d.]+/m, 'local MoveDistance = ' + grid + '.0'));
await writeFile(scenePath, JSON.stringify(scene, null, 4) + '\n');
console.log('Kenney rebuild: ' + actors.length + ' actors; ' + roadRows.size + ' single-lane road rows, one car per road row.');
console.log('Grid/move: ' + grid + '; pig length: ' + playerLength + '; car width: ' + vehicleWidth + '; rows: ' + rowCount + '.');

}
