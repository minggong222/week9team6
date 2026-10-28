local function Vector(X, Y, Z) return { X = X or 0, Y = Y or 0, Z = Z or 0 } end
local Components, Root = {}, {}
local Actor = { FindComponentByName = function(_, Name) return Components[Name] end,
    GetRootComponent = function() return Root end }
function Actor:AddComponent(_, Name)
    local C = {}
    function C:SetupAttachment() end
    function C:SetStaticMesh() end
    function C:SetCollisionEnabled() end
    function C:SetRelativeRotation() end
    function C:SetRelativeScale3D(V) self.Scale = V end
    function C:SetVisibility(V) self.Visible = V end
    function C:SetRelativeLocation(V) self.Position = V end
    function C:GetRelativeLocation() return self.Position end
    function C:GetWorldLocation() return self.Position end
    function C:CalcBounds() return { Min = Vector(0, -1.775, 0), Max = Vector(0, 1.775, 1) } end
    Components[Name] = C
    return C
end
local Phase, Deaths = 'Playing', 0
local Level = { Invoke = function(_, Name)
    if Name == 'GetPhase' then return Phase end
    if Name == 'GetGroundZ' then return 0 end
    if Name == 'GetMovementBounds' then return -24, 24 end
    if Name == 'RequestGameOver' then Deaths = Deaths + 1 end
end }
local PlayerRoot = { SetWorldLocation = function(S, V) S.Position = V end,
    GetWorldLocation = function(S) return S.Position end }
local Player = { GetRootComponent = function() return PlayerRoot end }
local Env = setmetatable({ Actor = Actor, FVector = { new = Vector }, FRotator = { new = Vector },
    ECollisionEnabled = { NoCollision = 0 }, UClass = { Find = function(V) return V end },
    UStaticMeshComponent = { Cast = function(V) return V end },
    UAssetManager = { GetAssetByPath = function(_, P) return P end } }, { __index = _G })
setfenv(assert(loadfile('Assets/Scripts/World/RiverManager.lua')), Env)()
Env.Initialize({}, Level, Player, nil, { HasMapPool = true, RowCount = 3, HalfWidth = 24,
    CellSize = 3, Origin = Vector() })
local Config = dofile('Assets/Scripts/ConfigManager.lua').River
Env.ResetRandom(123)
Env.SpawnRow(1, 1)
local Logs, Lengths = Env.State.Slots[1].Logs, {}
for Index, Log in ipairs(Logs) do
    Lengths[Index] = Log.HalfLength
    if Index <= Config.MaxInitialLogsPerRow then
        assert(Log.Active and Log.Component.Visible and math.abs(Log.Component.Position.Y) < 24)
    else
        assert(not Log.Active and not Log.Component.Visible)
        assert(Log.Component.Position.Y == -Log.Lane.Direction * (24 + Log.HalfLength))
    end
    assert(Log.HalfLength >= Config.MinLogHalfLength and Log.HalfLength <= Config.MaxLogHalfLength)
    assert(math.abs(Log.Component.Scale.X / Config.LogScale.X * Log.BaseHalfLength - Log.HalfLength) < 0.000001)
    assert(Log.Component.Scale.Y == Config.LogScale.Y and Log.Component.Scale.Z == Config.LogScale.Z)
end
assert(Lengths[1] ~= Lengths[2])
Env.ResetRandom(123)
Env.SpawnRow(1, 1)
for Index, Log in ipairs(Logs) do assert(Log.HalfLength == Lengths[Index]) end
local Lane = Env.State.Slots[1]
assert(Lane.NextSpawnTime >= Config.MinSpawnDelay and Lane.NextSpawnTime <= Config.MaxSpawnDelay)
local Waiting = Logs[Config.MaxInitialLogsPerRow + 1]
local Entry = -Lane.Direction * (24 + Waiting.HalfLength)
Lane.NextSpawnTime = 0
Env.Update(0)
assert(Waiting.Active and Waiting.Component.Visible and Waiting.Component.Position.Y == Entry)
Env.Update(0.1)
assert(Waiting.Component.Position.Y == Entry + Lane.Direction * Lane.Speed * 0.1)
Env.SpawnRow(2, 2)
Env.SpawnRow(3, 3)
assert(Env.State.Slots[2].Direction == -Lane.Direction)
assert(Env.State.Slots[3].Direction == Lane.Direction)
Env.RemoveRow(2)
Env.SpawnRow(2, 2)
assert(Env.State.Slots[2].Direction == -Lane.Direction)
for Index, Log in ipairs(Logs) do Log.Component.Position = Vector(3, Index == 1 and 0 or 100, 0) end
local Limit = Logs[1].HalfLength + Config.PlayerHalfWidth
assert(Env.ResolveLanding(Vector(3, Limit - 0.01, 0)))
Env.DetachPlayer()
assert(not Env.ResolveLanding(Vector(3, Limit + 0.01, 0)) and Deaths == 1)
Phase = 'Dead'
local RandomState = Env.State.RandomState
Logs[1].Component.Position.Y = Lane.Direction * 1000
Env.Update(0.01)
assert(Env.State.RandomState ~= RandomState and Logs[1].HalfLength ~= Lengths[1])
assert(math.abs(Logs[1].Component.Position.Y) == 24 + Logs[1].HalfLength)
assert(not Logs[1].Active and not Logs[1].Component.Visible)
assert(Logs[1].Component.Position.Y == -Lane.Direction * (24 + Logs[1].HalfLength))
-- 생성 시간이 되어도 진입 지점이 막혀 있으면 기다린다.
local Waiting, Blocking = Logs[1], Logs[2]
for _, Log in ipairs(Logs) do Log.Active = false end
Blocking.Active = true
Blocking.Component.Position = Vector(3, Waiting.Component.Position.Y, 0)
Lane.NextSpawnTime = 0
Env.State.Slots[1].Speed = 0
Env.Update(0.01)
assert(not Waiting.Active)
Blocking.Active = false
Env.Update(0.01)
assert(Waiting.Active and Waiting.Component.Visible)
print('PASS river initial population, edge-only timed spawning, alternating river directions, random lengths and landing')
