import { readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
// --main combines __Crossy's player/camera/HUD with the pooled map and traffic.
const mainGame = process.argv.includes('--main');
const outputPath = 'Scenes/' + (mainGame ? '0_Main_Game.scene' : 'maptest.scene');
const scene = JSON.parse(await readFile(root + 'Scenes/' + (mainGame ? '__Crossy.scene' : 'crossy2.scene'), 'utf8'));
// Preserve the user's player/camera/light edits when refreshing the generated map.
let existingScene;
try { existingScene = JSON.parse(await readFile(root + outputPath, 'utf8')); }
catch (e) { if (e.code !== 'ENOENT') throw e; }
// Read the editor preview settings from the same Lua constants used at runtime.
const settings = await readFile(root + 'Assets/Scripts/LevelManager.lua', 'utf8');
function numberSetting(name) {
    const match = settings.match(new RegExp('^local ' + name + ' = (-?[0-9.]+)', 'm'));
    if (!match) throw new Error('Missing numeric map setting: ' + name);
    return Number(match[1]);
}
const grid = numberSetting('LaneSpacing'), behind = numberSetting('RowsBehind'), ahead = numberSetting('RowsAhead');
const initialSeed = numberSetting('InitialSeed');
const halfWidth = numberSetting('MapPlayerHalfWidth'), boundaryWidth = numberSetting('MapBoundaryWidth');
const roadChance = numberSetting('MapRoadChance'), treesPerRow = numberSetting('TreesPerRow');
const railChance = numberSetting('MapRailChance');
const signalSpacing = numberSetting('MapRailSignalSpacing'), signalOffsetX = numberSetting('MapRailSignalOffsetX');
const trainTypeText = settings.match(/^local MapTrainTypes = \{([\s\S]*?)^\}/m)?.[1];
const trainPaths = [...(trainTypeText ?? '').matchAll(/Path = "([^"]+)"/g)].map(m => m[1]);
if (!trainPaths.length) throw new Error('Missing MapTrainTypes.');
const weightText = settings.match(/^local MapTreeCountWeights = \{([^}]+)\}/m)?.[1];
if (!weightText) throw new Error('Missing MapTreeCountWeights.');
const treeWeights = weightText.replace(/--[^\r\n]*/g, '').split(',').filter(v => v.trim()).map(Number);
const totalTreeWeight = treeWeights.reduce((sum, weight) => sum + weight, 0);
if (!(roadChance >= 0 && railChance >= 0 && roadChance + railChance <= 1) || !(halfWidth >= grid && halfWidth <= 33) || !(boundaryWidth > 0) || !(signalSpacing > 0))
    throw new Error('Invalid map width, road probability or signal spacing.');
if (treeWeights.length > treesPerRow + 1 || !treeWeights.every(v => Number.isFinite(v) && v >= 0) || !(totalTreeWeight > 0))
    throw new Error('Invalid tree count weights.');
const poolCount = behind + ahead + 1;
const sourceActors = scene.World.PersistentLevel.Actors;
const meshOf = actor => actor.Components.find(c => c.Properties.StaticMesh);
function meshTemplate(name) {
    const actor = sourceActors.find(a => meshOf(a)?.Properties.StaticMesh.endsWith('/' + name + '.obj'));
    if (!actor) throw new Error('Missing source mesh: ' + name);
    return meshOf(actor);
}
async function boundsFor(path) {
    const points = (await readFile(root + 'Assets/' + path, 'utf8')).split(/\r?\n/)
        .filter(line => /^v\s/.test(line)).map(line => {
            const [x, y, z] = line.trim().split(/\s+/).slice(1, 4).map(Number);
            return [-z, x, y];
        });
    const min = [0, 1, 2].map(i => Math.min(...points.map(p => p[i])));
    const max = [0, 1, 2].map(i => Math.max(...points.map(p => p[i])));
    return { min, size: max.map((v, i) => v - min[i]), center: max.map((v, i) => (v + min[i]) / 2) };
}
const transform = (x = 0, y = 0, z = 0, yaw = 0, scale = 1) => ({
    Location: { X: x, Y: y, Z: z }, Rotation: { Pitch: 0, Yaw: yaw, Roll: 0 },
    Scale: { X: scale, Y: scale, Z: scale }
});
function sceneComponent(name, parent, pose, visible = true) {
    return { Class: 'USceneComponent', Name: name, Parent: parent, Properties: {
        Transform: pose, bAutoActivate: true, bVisible: visible, bHiddenInGame: false
    } };
}
function meshComponent(template, name, parent, pose) {
    const component = structuredClone(template);
    component.Name = name;
    component.Parent = parent;
    component.Properties.Transform = pose;
    component.Properties.bVisible = true;
    component.Properties.bGenerateOverlapEvents = false;
    component.OverrideMaterials = [];
    return component;
}

const grass = meshTemplate('kenney-grass-row');
const road = meshTemplate('kenney-road-row');
const rail = structuredClone(grass);
rail.Properties.StaticMesh = 'Models/Crossy/Trains/crossy-rail-row.obj';
const signalTemplate = structuredClone(grass);
signalTemplate.Properties.StaticMesh = 'Models/Crossy/Signals/crossy-rail-signal-off.obj';
const trains = trainPaths.map(path => {
    const template = structuredClone(grass);
    template.Properties.StaticMesh = path;
    return template;
});
const treeStyles = [];
for (const name of ['kenney-tree-019', 'kenney-tree-020']) {
    const template = meshTemplate(name), bounds = await boundsFor(template.Properties.StaticMesh);
    const scale = 2.2 / Math.max(bounds.size[0], bounds.size[1]);
    treeStyles.push({ template, pose: transform(-bounds.center[0] * scale, -bounds.center[1] * scale,
        -bounds.min[2] * scale, 0, scale) });
}

// Same deterministic preview as LevelManager's map-only mode.
let seed = initialSeed, safeColumn = 0;
let trainTypeSeed = (initialSeed + 65537) % 2147483647;
const pickTrain = () => {
    trainTypeSeed = trainTypeSeed * 48271 % 2147483647;
    return trains[Math.floor(trainTypeSeed / 2147483647 * trains.length)];
};
const random = () => { seed = seed * 48271 % 2147483647; return seed / 2147483647; };
const integer = (min, max) => min + Math.floor(random() * (max - min + 1));
function rowType(index) {
    if (index < 3) return 'grass';
    const choice = random();
    if (choice < railChance) return 'rail';
    return choice < railChance + roadChance ? 'road' : 'grass';
}
function treeCount() {
    let choice = random() * totalTreeWeight;
    for (let count = 0; count < treeWeights.length; count++) {
        choice -= treeWeights[count];
        if (choice < 0) return count;
    }
    return treeWeights.length - 1;
}
const existingActors = existingScene?.World.PersistentLevel.Actors ?? sourceActors;
const managerName = mainGame ? 'LevelManager' : 'MapTestLevelManager';
const existingManager = existingScene?.World.PersistentLevel.Actors.find(a => a.Name === managerName);
const playerTemplate = existingActors.find(a => a.Name === 'Player');
const playerOrigin = playerTemplate?.Components.find(c => c.Name === 'DefaultSceneRoot')?.Properties.Transform.Location;
const rootPose = existingManager?.Components.find(c => c.Name === 'DefaultSceneRoot')?.Properties.Transform
    ?? transform(mainGame ? playerOrigin.X : 0, mainGame ? playerOrigin.Y : 0);
const components = [sceneComponent('DefaultSceneRoot', '', structuredClone(rootPose))];
let previewRailRows = 0;
for (let slot = 1; slot <= poolCount; slot++) {
    const index = slot - behind - 1, kind = rowType(index);
    if (kind === 'rail') previewRailRows++;
    const floor = meshComponent(kind === 'rail' ? rail : (kind === 'road' ? road : grass), 'MapFloor_' + slot,
        'DefaultSceneRoot', transform(index * grid));
    // 잔디는 MapBackdrop으로 표시하고 도로·철길 타일만 렌더링한다.
    floor.Properties.bVisible = kind !== 'grass';
    components.push(floor);
    const train = kind === 'rail' ? pickTrain() : trains[0];
    const trainComponent = meshComponent(train, 'MapTrain_' + slot, 'DefaultSceneRoot', transform(index * grid, -16));
    trainComponent.Properties.bVisible = kind === 'rail';
    components.push(trainComponent);
    if (kind === 'rail') {
        const halfSignals = Math.floor(halfWidth / signalSpacing);
        for (let i = 1; i <= halfSignals * 2 + 1; i++) {
            const name = 'RailSignal_' + slot + '_' + i;
            components.push(meshComponent(signalTemplate, name, 'DefaultSceneRoot',
                transform(index * grid + signalOffsetX, (i - halfSignals - 1) * signalSpacing + grid / 2, 0, 270)));
            components.push({ Class: 'UPointLightComponent', Name: name + '_Light', Parent: name,
                Properties: { Transform: transform(0, -0.24, 2.23), Intensity: 0,
                    AttenuationRadius: 2.5, LightColor: 4278982911, bCastShadows: false,
                    bVisible: false, bHiddenInGame: false, bAutoActivate: true } });
        }
    }
    const placements = [];
    if (kind === 'grass' && index >= 3) {
        const halfColumns = Math.floor(halfWidth / grid), routeLimit = Math.max(0, halfColumns - 2);
        const next = Math.max(-routeLimit, Math.min(routeLimit, safeColumn + integer(-1, 1)));
        const columns = Array.from({ length: halfColumns * 2 + 1 }, (_, i) => i - halfColumns)
            .filter(c => c < Math.min(safeColumn, next) || c > Math.max(safeColumn, next));
        safeColumn = next;
        for (let i = columns.length - 1; i > 0; i--) {
            const j = integer(0, i); [columns[i], columns[j]] = [columns[j], columns[i]];
        }
        const count = Math.min(treeCount(), columns.length);
        for (let i = 0; i < count; i++) placements.push(transform(index * grid, columns[i] * grid,
            0, integer(0, 3) * 90, 0.9 + random() * 0.1));
    }
    for (let tree = 1; tree <= treesPerRow; tree++) {
        const name = 'MapTree_' + slot + '_' + tree;
        const style = treeStyles[(slot + tree) % treeStyles.length];
        components.push(sceneComponent(name, 'DefaultSceneRoot', placements[tree - 1] ?? transform(),
            tree <= placements.length));
        components.push(meshComponent(style.template, 'MapTreeMesh_' + slot + '_' + tree,
            name, structuredClone(style.pose)));
    }
}

const centerX = (ahead - behind) * grid / 2;
const backdropTemplate = meshTemplate('kenney-backdrop');
const backdrop = meshComponent(backdropTemplate, 'MapBackdrop', 'DefaultSceneRoot', transform(centerX, 0, 0.01));
backdrop.Properties.Transform.Scale.X = (poolCount + 4) * grid /
    (await boundsFor(backdrop.Properties.StaticMesh)).size[0];
components.push(backdrop);
const sidewalkTemplate = meshTemplate('kenney-sidewalk');
for (const side of [-1, 1]) {
    const sidewalk = meshComponent(sidewalkTemplate, side < 0 ? 'MapBoundaryLeft' : 'MapBoundaryRight',
        'DefaultSceneRoot', transform(centerX, side * (halfWidth + grid / 2 + boundaryWidth / 2), 0.01));
    sidewalk.Properties.Transform.Scale.X = poolCount * grid /
        (await boundsFor(sidewalk.Properties.StaticMesh)).size[0];
    sidewalk.Properties.Transform.Scale.Y = boundaryWidth /
        (await boundsFor(sidewalk.Properties.StaticMesh)).size[1];
    components.push(sidewalk);
}
components.push({ Class: 'ULuaComponent', Name: 'LevelManager', Parent: '', Properties: {
    ScriptPath: 'Scripts/LevelManager.lua', bAutoActivate: true, bVisible: true, bHiddenInGame: false
} });
const player = existingActors.find(a => a.Name === 'Player');
if (!player) throw new Error('Player is missing.');
if (!existingScene) {
    const playerRoot = player.Components.find(c => c.Name === 'DefaultSceneRoot').Properties.Transform;
    playerRoot.Location = { X: 0, Y: 0, Z: playerRoot.Location.Z };
}
const actors = [player, ...existingActors.filter(a => /LightActor$/.test(a.Class) || a.Class === 'ACameraActor'), {
    Class: 'AActor', Name: managerName, Properties: { bActorEnableCollision: false }, Components: components
}];
if (mainGame) {
    const gameManager = existingActors.find(a => a.Components.some(c => c.Properties.ScriptPath === 'Scripts/GameManager.lua'));
    if (!gameManager) throw new Error('__Crossy GameManager/HUD is missing.');
    actors.push(gameManager);
}
const outputScene = existingScene ?? scene;
outputScene.World.PersistentLevel.Actors = actors;
await writeFile(root + outputPath, JSON.stringify(outputScene, null, 4) + '\n');
console.log(JSON.stringify({ output: outputPath, actors: actors.length,
    rows: poolCount, behind, ahead, treePool: poolCount * treesPerRow,
    railRows: previewRailRows, trainPool: poolCount, cars: mainGame ? '100 pooled at runtime' : 0 }));
