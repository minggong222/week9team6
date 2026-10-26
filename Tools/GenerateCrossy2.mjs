import { readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const sourcePath = root + 'Scenes/Crossy.scene';
const outputPath = root + 'Scenes/crossy2.scene';
const grid = 3, rowCount = 100, halfColumns = 8;
const seed = Number(process.argv.find(arg => arg.startsWith('--seed='))?.slice(7) ?? 20261026);
if (!Number.isInteger(seed) || seed < 0 || seed > 0xffffffff) throw new Error('Seed must be a uint32.');
let randomState = seed;
function random() {
    randomState = (randomState + 0x6d2b79f5) >>> 0;
    let value = Math.imul(randomState ^ randomState >>> 15, randomState | 1);
    value ^= value + Math.imul(value ^ value >>> 7, value | 61);
    return ((value ^ value >>> 14) >>> 0) / 4294967296;
}
const integer = (min, max) => min + Math.floor(random() * (max - min + 1));
const choose = values => values[integer(0, values.length - 1)];
function shuffle(values) {
    for (let i = values.length - 1; i > 0; i--) {
        const j = integer(0, i);
        [values[i], values[j]] = [values[j], values[i]];
    }
    return values;
}
const scene = JSON.parse(await readFile(sourcePath, 'utf8'));
const sourceActors = scene.World.PersistentLevel.Actors;
const rootTransform = actor => actor.Components.find(c => c.Name === 'DefaultSceneRoot').Properties.Transform;
const meshComponent = actor => actor.Components.find(c => c.Properties.StaticMesh);
function templateFor(meshName) {
    const actor = sourceActors.find(a => meshComponent(a)?.Properties.StaticMesh.endsWith('/' + meshName + '.obj'));
    if (!actor) throw new Error('Missing source template: ' + meshName);
    return actor;
}
function placed(template, name, x, y, yaw = 0) {
    const actor = structuredClone(template);
    actor.Name = name;
    const transform = rootTransform(actor);
    transform.Location.X = x;
    transform.Location.Y = y;
    transform.Rotation.Yaw = yaw;
    return actor;
}
const boundsCache = new Map();
async function boundsFor(path) {
    if (boundsCache.has(path)) return boundsCache.get(path);
    const points = (await readFile(root + 'Assets/' + path, 'utf8')).split(/\r?\n/)
        .filter(line => /^v\s/.test(line)).map(line => {
            const [x, y, z] = line.trim().split(/\s+/).slice(1, 4).map(Number);
            return [-z, x, y];
        });
    if (!points.length) throw new Error('No vertices in ' + path);
    const min = [0, 1, 2].map(axis => Math.min(...points.map(p => p[axis])));
    const max = [0, 1, 2].map(axis => Math.max(...points.map(p => p[axis])));
    const result = { min, size: max.map((v, axis) => v - min[axis]),
        center: max.map((v, axis) => (v + min[axis]) / 2) };
    boundsCache.set(path, result);
    return result;
}

const grassTemplate = templateFor('kenney-grass-row');
const roadTemplate = templateFor('kenney-road-row');
const treeTemplates = ['kenney-tree-019', 'kenney-tree-020'].map(templateFor);
const carTemplates = sourceActors.filter(a => /^Car_L\d+_\d+$/.test(a.Name));
if (!carTemplates.length) throw new Error('No source cars.');
// Use complete long-vehicle models from Kenney Car Kit, preserving their original proportions.
const vehicleTemplates = [];
for (const model of ['sedan', 'taxi', 'firetruck', 'garbage-truck', 'delivery', 'ambulance']) {
    const path = 'Models/Crossy/Cars/' + model + '.obj';
    const template = structuredClone(carTemplates.find(a => meshComponent(a).Properties.StaticMesh === path) ?? carTemplates[0]);
    const mesh = meshComponent(template);
    const bounds = await boundsFor(path);
    const scale = 2 / bounds.size[1];
    mesh.Properties.StaticMesh = path;
    mesh.OverrideMaterials = [];
    mesh.Properties.Transform.Scale = { X: scale, Y: scale, Z: scale };
    mesh.Properties.Transform.Location = { X: -bounds.center[0] * scale,
        Y: -bounds.center[1] * scale, Z: -bounds.min[2] * scale };
    vehicleTemplates.push(template);
}
// Existing generated map objects are replaced; Player, lighting and custom actors are copied.
const generatedNames = /^(Ground_|Grass_|Road_|LaneMark_|Tree_|Boundary_|Car_L\d+_\d+$|Rock_|Bench_|Cone_|Start_Line$|Finish_Tile_)/;
const actors = sourceActors.filter(a => !generatedNames.test(a.Name));
const player = actors.find(a => a.Name === 'Player');
if (!player) throw new Error('Player is missing.');
rootTransform(player).Location = { X: 0, Y: 0, Z: rootTransform(player).Location.Z };
if (!/^local MoveDistance = 3(?:\.0)?\s*$/m.test(await readFile(root + 'Assets/Scripts/Player.lua', 'utf8')))
    throw new Error('Player.lua must use a movement distance of 3.');

// Random road runs of 1–4 rows alternate with grass runs of 1–3 rows.
// The first and last three rows stay safe grass.
const rowTypes = Array(rowCount).fill('grass');
for (let row = 3; row < rowCount - 3;) {
    const roadEnd = Math.min(rowCount - 3, row + integer(1, 4));
    while (row < roadEnd) rowTypes[row++] = 'road';
    row += integer(1, 3);
}

// Extend only this scene's transforms; shared meshes and Crossy.scene are left untouched.
const centerX = (rowCount - 1) * grid / 2;
const backdrop = placed(templateFor('kenney-backdrop'), 'Ground_Backdrop', centerX, 0);
const backdropMesh = meshComponent(backdrop);
backdropMesh.Properties.Transform.Scale.X = (rowCount + 4) * grid /
    (await boundsFor(backdropMesh.Properties.StaticMesh)).size[0];
actors.push(backdrop);
for (const side of [-1, 1]) {
    const sidewalk = placed(templateFor('kenney-sidewalk'),
        side < 0 ? 'Boundary_Left' : 'Boundary_Right', centerX, side * (halfColumns + 1) * grid);
    const mesh = meshComponent(sidewalk);
    mesh.Properties.Transform.Scale.X = rowCount * grid / (await boundsFor(mesh.Properties.StaticMesh)).size[0];
    actors.push(sidewalk);
}

let roadCount = 0, treeCount = 0, safeColumn = 0;
for (let row = 0; row < rowCount; row++) {
    if (rowTypes[row] === 'road') {
        actors.push(placed(roadTemplate, 'Road_Row_' + row, row * grid, 0));
        const car = placed(choose(vehicleTemplates), 'Car_L' + (++roadCount) + '_1',
            row * grid, integer(-halfColumns, halfColumns) * grid, choose([-90, 90]));
        actors.push(car);
        continue;
    }

    actors.push(placed(grassTemplate, 'Grass_' + row, row * grid, 0));
    if (row < 3 || row >= rowCount - 3) continue;
    // Keep connected empty cells through consecutive grass rows, including horizontal turns.
    const nextColumn = Math.max(-halfColumns + 2, Math.min(halfColumns - 2, safeColumn + integer(-1, 1)));
    const candidates = shuffle(Array.from({ length: halfColumns * 2 + 1 }, (_, i) => i - halfColumns)
        .filter(column => column < Math.min(safeColumn, nextColumn) || column > Math.max(safeColumn, nextColumn)));
    safeColumn = nextColumn;
    for (const column of candidates.slice(0, integer(3, 7))) {
        const tree = placed(choose(treeTemplates), 'Tree_R' + row + '_C' + (column + halfColumns),
            row * grid, column * grid, integer(0, 3) * 90);
        const mesh = meshComponent(tree);
        const bounds = await boundsFor(mesh.Properties.StaticMesh);
        // The whole canopy stays inside its grass cell, away from adjacent road lanes.
        const scale = (2 + random() * 0.2) / Math.max(bounds.size[0], bounds.size[1]);
        mesh.Properties.Transform.Scale = { X: scale, Y: scale, Z: scale };
        mesh.Properties.Transform.Location = { X: -bounds.center[0] * scale,
            Y: -bounds.center[1] * scale, Z: -bounds.min[2] * scale };
        actors.push(tree);
        treeCount++;
    }
}
scene.World.PersistentLevel.Actors = actors;
await writeFile(outputPath, JSON.stringify(scene, null, 4) + '\n');
console.log(JSON.stringify({ output: 'Scenes/crossy2.scene', seed, rows: rowCount, grid,
    roadRows: roadCount, grassRows: rowCount - roadCount, cars: roadCount, trees: treeCount,
    actors: actors.length, startX: 0, endX: (rowCount - 1) * grid }));
