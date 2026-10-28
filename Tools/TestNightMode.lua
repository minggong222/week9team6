-- 저장소 루트에서 Source/ThirdParty/luajit/luajit.exe Tools/TestNightMode.lua 실행
local function Vector(X, Y, Z) return { X = X or 0, Y = Y or 0, Z = Z or 0 } end
local function Component()
    local C = { Position = Vector(), Children = {} }
    function C:SetupAttachment(Parent)
        self.Parent = Parent
        Parent.Children[#Parent.Children + 1] = self
    end
    function C:GetName() return self.Name or "" end
    function C:SetRelativeLocation(V) self.Position = V end
    function C:GetRelativeLocation() return self.Position end
    function C:SetRelativeRotation(V) self.Rotation = V end
    function C:SetLightColor(V) self.Color = V end
    function C:SetShadowSlopeBias(V) self.ShadowSlopeBias = V end
    function C:SetInnerConeAngle(V) self.InnerConeAngle = V end
    function C:SetOuterConeAngle(V) self.OuterConeAngle = V end
    function C:SetIntensity(V) self.Intensity = V end
    function C:SetAttenuationRadius(V) self.Radius = V end
    function C:SetCastShadows(V) self.Shadows = V end
    function C:SetStaticMesh(V) self.Mesh = V end
    function C:SetVisibility(V, Propagate)
        self.Visible = V
        if Propagate then
            for _, Child in ipairs(self.Children) do Child:SetVisibility(V, true) end
        end
    end
    function C:CalcBounds()
        return { Min = Vector(-1, self.Position.Y - 10, 0), Max = Vector(1, self.Position.Y + 10, 3) }
    end
    return C
end

local Components, Root = {}, Component()
local Actor = {}
function Actor:GetRootComponent() return Root end
function Actor:GetComponents() return {} end
function Actor:FindComponentByName(Name)
    if Name:find('^MapTrain_') and not Components[Name] then Components[Name] = Component() end
    return Components[Name]
end
function Actor:AddComponent(_, Name)
    Components[Name] = Component()
    return Components[Name]
end

local Base = setmetatable({ Actor = Actor, FVector = { new = Vector },
    FTransform = { new = function() return {} end },
    FRotator = { new = function(P, Y, R) return { Pitch = P, Yaw = Y, Roll = R } end },
    FColor = { new = function(...) return {...} end }, UClass = { Find = function(V) return V end },
    UAssetManager = { GetAssetByPath = function(_, P) return P end },
    Audio = { PlaySFX = function() end },
    UBoxComponent = { Cast = function() return nil end }
}, { __index = _G })
for _, Name in ipairs({ 'UStaticMeshComponent', 'UPointLightComponent', 'USpotLightComponent' }) do
    Base[Name] = { Cast = function(V) return V end }
end
local function Load(Path)
    local Env = setmetatable({}, { __index = Base })
    setfenv(assert(loadfile('Assets/Scripts/' .. Path)), Env)()
    return Env
end

local Defaults = dofile("Assets/Scripts/ConfigManager.lua")
assert(not Defaults.Level.NightMode and Defaults.Game.DefaultMode == "Day")
local Independent = dofile("Assets/Scripts/ConfigManager.lua")
Independent.Level.Lighting.Day.Car.Intensity = 99
assert(Defaults.Level.Lighting.Day.Car.Intensity == 0)

local Random = 0
-- 엔진이 제공하는 math.random을 실제 플레이 시드의 입력으로 사용하는지 확인한다.
local SeedCalls = 0
Base.math = setmetatable({ random = function(Min, Max)
    SeedCalls = SeedCalls + 1
    assert(Min == 1 and Max < 2147483647 - 104729)
    return 12345 + SeedCalls
end }, { __index = math })
local FirstRun = Load("LevelManager.lua")
local SecondRun = Load("LevelManager.lua")
assert(FirstRun.State.Seed == 12346 and SecondRun.State.Seed == 12347)
Base.math = nil
local Lighting = Load("LevelManager.lua")
assert(Lighting.State.LightingProfile == "Day", "Default mode must be Day")
Lighting.TickComponent({}, 0) -- 초기화 전 Tick은 다른 상태에 접근하지 않는다.
Lighting.SetLightingProfile("Night")
local Level = { Invoke = function(_, Name, ...)
    if Name == "RegisterLight" then return Lighting.RegisterLight(...) end
    if Name == 'TrafficRandom' then return Random end
    if Name == 'GetPhase' then return 'Ready' end
end }
local Layout = { NightMode = true, HasMapPool = true, RoadHalfWidth = 34,
    CellSize = 3, RowCount = 1, HalfWidth = 24, Origin = Vector() }
local Traffic = Load('World/TrafficManager.lua')
Traffic.Initialize({}, Level, {}, nil, Layout)
local _, Speed = Traffic.PrepareRow(1, 1, 'road')
assert(math.abs(Speed - Defaults.Traffic.Night.MinSpeed) < 0.0001)
Random = 1
_, Speed = Traffic.PrepareRow(1, 1, 'road')
assert(Speed == Defaults.Traffic.Night.MaxSpeed)
assert(Traffic.State.Lanes[1].NextSpawnTime == Defaults.Traffic.Night.MaxSpawnInterval)

-- 차량 풀을 실제로 생성해 모든 차체에 전조등이 부착되는지 확인한다.
local Cars = {}
local World = {}
function Actor:GetWorld() return World end
function World:SpawnActor(_, _, Pose)
    local Car = { Root = Component(), Mesh = Component(), Lights = {} }
    Car.Root.Position = Pose.Location
    function Car:GetActorLocation() return self.Root.Position end
    function Car:GetRootComponent() return self.Root end
    function Car:FindComponentByName() return self.Mesh end
    function Car.Mesh:SetRelativeScale3D(V) self.Scale = V end
    function Car:AddComponent(Class)
        if Class == 'USpotLightComponent' then
            local Light = Component()
            self.Lights[#self.Lights + 1] = Light
            return Light
        end
        local Script = {}
        function Script:SetScriptPath() return true end
        function Script:InitializeLua() end
        function Script:LoadLua() return true end
        function Script:Invoke() end
        return Script
    end
    function Car:GetUUID() return #Cars end
    Cars[#Cars + 1] = Car
    return Car
end
Base.ULuaComponent = { Cast = function(V) return V end, StaticClass = function() return 'ULuaComponent' end }
Traffic.State.Lanes = {}
Traffic.EnableTraffic()
assert(#Cars == 100)
for _, Car in ipairs(Cars) do
    assert(#Car.Lights == 2)
    assert(Car.Lights[1].Position.Y == -Car.Lights[2].Position.Y)
    assert(Car.Lights[1].Position.Y ~= 0)
    for _, Light in ipairs(Car.Lights) do
        assert(Light.ShadowSlopeBias == 0.05 and Light.InnerConeAngle == 30 and Light.OuterConeAngle == 40)
        assert(Light.Radius == 15)
        assert(Light.Parent == Car.Root and Light.Position.X < 0)
        assert(Light.Rotation.Yaw == 180 and Light.Intensity == 4 and not Light.Shadows)
    end
end

local Train = Load('World/TrainManager.lua')
local PlayerPosition = Vector(1000, 0, 0)
local Player = { GetActorLocation = function() return PlayerPosition end,
    FindComponentByName = function() return nil end,
    TryGetActorBounds = function() return false end }
Random = 0
Train.Initialize(Level, Player, Layout)
Train.SpawnRail(1, 8, true)
local Light = assert(Components.NightTrainLight_1_1)
local RightLight = assert(Components.NightTrainLight_1_2)
for _, Headlight in ipairs({ Light, RightLight }) do
    assert(Headlight.ShadowSlopeBias == 0.05 and Headlight.InnerConeAngle == 30 and Headlight.OuterConeAngle == 40)
end
assert(Light.Position.X == -RightLight.Position.X and Light.Position.X ~= 0)
assert(RightLight.Parent == Components.MapTrain_1 and RightLight.Position.Y == 10)
assert(Light.Parent == Components.MapTrain_1 and Light.Position.Y == 10)
assert(Light.Rotation.Yaw == 90 and Light.Shadows == false and Light.Visible == false)
assert(Train.GetLaneInfo(1).Speed == 65)
Train.Update(Train.GetLaneInfo(1).SecondsUntilSpawn)
assert(Light.Visible == true and RightLight.Visible == true)
Train.Update(100)
assert(Light.Visible == false and RightLight.Visible == false)
assert(Train.GetLaneInfo(1).SecondsUntilSpawn == math.max(Defaults.Train.Night.WarningTime, Defaults.Train.Night.WaitMin))
Train.RemoveRow(1)
Train.SpawnRail(1, 20, false)
assert(Components.NightTrainLight_1_1 == Light and Components.NightTrainLight_1_2 == RightLight and Light.Position.Y == 10)
assert(Components.MapTrain_1.Rotation.Yaw == 180 and not Light.Visible)
Train.RemoveRow(1)
assert(not Light.Visible)
Lighting.SetLighting("Car", 7, 12, 34, 56, 255)
for _, Car in ipairs(Cars) do
    for _, Light in ipairs(Car.Lights) do
        assert(Light.Intensity == 7 and Light.Color[2] == 34 and Light.Radius == 15)
    end
end
Lighting.SetLighting("Train", 9, 40, 50, 60, 255)
assert(Light.Intensity == 9 and Light.Color[3] == 60 and not Light.Visible)
assert(RightLight.Intensity == 9 and RightLight.Color[3] == 60 and not RightLight.Visible)
local Sky, Sun = Component(), Component()
Lighting.RegisterLight("Sky", Sky)
Lighting.RegisterLight("Sun", Sun)
assert(Sky.Intensity == Defaults.Level.Lighting.Night.Sky.Intensity
    and Sun.Intensity == Defaults.Level.Lighting.Night.Sun.Intensity)
Lighting.SetLighting("Sky", 0.3, 100, 120, 140, 255)
Lighting.SetLighting("Sun", 0.6, 200, 210, 220, 255)
assert(Sky.Intensity == 0.3 and Sky.Color[1] == 100)
assert(Sun.Intensity == 0.6 and Sun.Color[3] == 220)
local NewLight = Component()
Lighting.RegisterLight("Car", NewLight)
assert(NewLight.Intensity == 7 and NewLight.Color[3] == 56)
-- 낮 전환은 전조등 밝기만 끄고, 밤으로 돌아오면 밤에서 편집한 값을 복원한다.
Lighting.SetLightingProfile("Day")
assert(Sky.Intensity == 0.2 and Sun.Intensity == 0.5)
assert(Light.Intensity == 0 and NewLight.Intensity == 0)
Lighting.SetLighting("Sun", 0.7, 250, 240, 230)
Lighting.SetLightingProfile("Night")
assert(Sky.Intensity == 0.3 and Sun.Intensity == 0.6)
assert(Light.Intensity == 9 and NewLight.Intensity == 7)
Lighting.SetLightingProfile("Day")
assert(Sun.Intensity == 0.7 and Sun.Color[2] == 240)
local DayLight = Component()
Lighting.RegisterLight("Car", DayLight)
assert(DayLight.Intensity == 0)
Lighting.SetLightingProfile("Night")
assert(DayLight.Intensity == 7)
-- 근처 기차의 경고음은 진입 시 한 번만 재생한다.
local WarningCount, WarningVolume = 0, nil
Base.Audio.PlaySFX = function(Path, Volume)
    if Path == "Audio/bell_deng_ge_deng_v2.wav" then
        WarningCount, WarningVolume = WarningCount + 1, Volume
    end
end
PlayerPosition = Vector(0, 0, 0)
Train.SpawnRail(1, 8, true)
Train.Update(Train.GetLaneInfo(1).SecondsUntilSpawn - 0.5)
assert(WarningCount == 1 and WarningVolume == 0.5)
Train.Update(0.1)
assert(WarningCount == 1)
Train.RemoveRow(1)
PlayerPosition = Vector(1000, 0, 0)
assert(Defaults.Train.PassSoundLead == 30 and Defaults.Train.PassSoundVolume == 0.70)

-- 사망 압축은 실제 프레임 시간으로 진행하고 원래 자세를 복원한다.
local Reaction = Load("Effects/PlayerHitReaction.lua")
local ReactionMesh = { Scale = Vector(1, 1, 1) }
function ReactionMesh:GetRelativeScale3D() return Vector(self.Scale.X, self.Scale.Y, self.Scale.Z) end
function ReactionMesh:SetRelativeScale3D(Value) self.Scale = Value end
function ReactionMesh:GetRelativeLocation() return Vector() end
function ReactionMesh:GetRelativeRotation() return {} end
function ReactionMesh:SetRelativeLocation() end
function ReactionMesh:SetRelativeRotation() end
Reaction.Initialize(ReactionMesh)
Reaction.PlayHitReaction()
Reaction.Update(0.06)
assert(ReactionMesh.Scale.Z < 1 and ReactionMesh.Scale.Z > 0.05)
Reaction.Update(0.06)
assert(math.abs(ReactionMesh.Scale.Z - 0.05) < 0.000001)
Reaction.ResetPlayerPose()
assert(ReactionMesh.Scale.Z == 1)

-- 피격 이동은 실제 시간으로 정확히 한 칸이며 가장자리·경계 사망은 이동하지 않는다.
local KnockbackRoot = { Position = Vector(6, 0, 0) }
function KnockbackRoot:GetWorldLocation() return self.Position end
function KnockbackRoot:SetWorldLocation(Value) self.Position = Value end
Reaction.Initialize(ReactionMesh, { GetRootComponent = function() return KnockbackRoot end })
Reaction.PlayHitReaction(Vector(0, 1, 0), 3, -24, 24)
Reaction.Update(Defaults.HitReaction.KnockbackDuration / 2)
assert(KnockbackRoot.Position.Y > 0 and KnockbackRoot.Position.Y < 3)
Reaction.Update(Defaults.HitReaction.KnockbackDuration)
assert(KnockbackRoot.Position.Y == 3)
Reaction.ResetPlayerPose()

for _, Direction in ipairs({ -1, 1 }) do
    KnockbackRoot.Position = Vector(6, 0, 0)
    Reaction.PlayHitReaction(Vector(0, Direction, 0), 3, -24, 24)
    Reaction.Update(Defaults.HitReaction.KnockbackDuration / 2)
    assert(math.abs(KnockbackRoot.Position.Y) > 0 and math.abs(KnockbackRoot.Position.Y) < 3)
    Reaction.Update(Defaults.HitReaction.KnockbackDuration)
    assert(KnockbackRoot.Position.X == 6 and KnockbackRoot.Position.Y == Direction * 3)
    Reaction.ResetPlayerPose()
end
-- 가장자리에서 안쪽으로 맞으면 한 칸 이동하고, 바깥쪽으로 맞으면 유지한다.
for _, Edge in ipairs({ -24, 24 }) do
    local Direction = Edge < 0 and 1 or -1
    KnockbackRoot.Position = Vector(6, Edge, 0)
    Reaction.PlayHitReaction(Vector(0, Direction, 0), 3, -24, 24)
    Reaction.Update(Defaults.HitReaction.KnockbackDuration)
    assert(KnockbackRoot.Position.Y == Edge + Direction * 3)
    Reaction.ResetPlayerPose()
end
for _, Edge in ipairs({ -24, 24, -23, 23 }) do
    KnockbackRoot.Position = Vector(6, Edge, 0)
    Reaction.PlayHitReaction(Vector(0, Edge < 0 and -1 or 1, 0), 3, -24, 24)
    Reaction.Update(1)
    assert(KnockbackRoot.Position.Y == Edge)
    Reaction.ResetPlayerPose()
end
KnockbackRoot.Position = Vector(6, 0, 0)
Reaction.PlayHitReaction(nil, 3, -24, 24)
Reaction.Update(1)
assert(KnockbackRoot.Position.Y == 0)
Reaction.PlayHitReaction(Vector(0, 1, 0), 3, -24, 24)
Reaction.Update(0.01)
Reaction.ResetPlayerPose()
KnockbackRoot.Position = Vector(0, 0, 0)
Reaction.Update(1)
assert(KnockbackRoot.Position.Y == 0)

-- 선택 화면 입력으로 난이도·조명이 전환되고 Shake가 한 번 실행되는지 확인한다.
Lighting.Refs.Layout = Layout
local ResetCount, ShakeCount = 0, 0
Lighting.ResetLevel = function(NewSeed)
    assert(NewSeed == false)
    ResetCount = ResetCount + 1
end
local Manager = Load("GameManager.lua")
Manager.State.Game.Phase, Manager.State.Game.Mode = "SelectingCharacter", "Night"
Manager.Refs.Level = { Invoke = function(_, Name, ...) return Lighting[Name](...) end }
Manager.Refs.Selection = { Invoke = function(_, Name)
    if Name == "HandleInput" then return false, false end
    if Name == "GetSelection" then return "Beaver", 1, 1 end
end }
function World:GetRealDeltaSeconds() return 0.016 end
Manager.Refs.HitReaction = { Invoke = function(_, Name, Delta)
    assert(Name == "Update" and Delta == 0.016)
    Reaction.Update(Delta)
end }
Manager.Refs.Camera = { Invoke = function() end }
Manager.Refs.Cinematics = { Invoke = function() end }
Manager.Refs.HUD = { Invoke = function(_, Name, _, _, _, _, Mode)
    assert(Name == "ShowSelection" and Mode == Manager.State.Game.Mode)
end }
function World:GetPlayerCameraManager()
    return { StartCameraShake = function(_, Duration)
        assert(Duration == 0.25)
        ShakeCount = ShakeCount + 1
    end }
end
local PressedKey = "Up"
Base.EKeyCode = { Up = "Up", Down = "Down" }
Base.Input = { IsKeyPressed = function(Key) return Key ~= nil and Key == PressedKey end }
Manager.TickComponent({}, 0)
assert(Manager.State.Game.Mode == "Day" and not Layout.NightMode)
assert(Sky.Intensity == 0.2 and ResetCount == 1 and ShakeCount == 1)
Random = 1
local _, DaySpeed = Traffic.PrepareRow(1, 1, "road")
assert(DaySpeed == 20 and Traffic.State.Lanes[1].NextSpawnTime == 2.5)
Train.SpawnRail(1, 8, true)
assert(Train.GetLaneInfo(1).Speed == 50)
PressedKey = nil
Manager.TickComponent({}, 0)
assert(ResetCount == 1 and ShakeCount == 1)
PressedKey = "Down"
Manager.TickComponent({}, 0)
assert(Manager.State.Game.Mode == "Night" and Layout.NightMode)
assert(ResetCount == 2 and ShakeCount == 2)
-- HitStop으로 게임 시간이 멈춘 프레임에도 R 입력이 암전 전에 시간 효과를 해제한다.
local Retry = Load("GameManager.lua")
Retry.State.Game.Phase = "Dead"
local TimeResetCount, FadeCount = 0, 0
function World:ResetTimeEffects() TimeResetCount = TimeResetCount + 1 end
Retry.Refs.HitReaction = { Invoke = function() end }
Retry.Refs.HUD = { Invoke = function() end }
Retry.Refs.Cinematics = { Invoke = function(_, Name)
    if Name == "PlayRestart" then
        assert(TimeResetCount == 1, "HitStop must end before FadeOut starts")
        FadeCount = FadeCount + 1
        return 0.15
    end
end }
Base.EKeyCode.R = "R"
PressedKey = "R"
Retry.TickComponent({}, 0)
assert(Retry.State.Restarting and FadeCount == 1 and TimeResetCount == 1)
PressedKey = nil
Retry.TickComponent({}, 0.1)
assert(Retry.State.Restarting and FadeCount == 1)
Retry.TickComponent({}, 0.06)
assert(not Retry.State.Restarting and Retry.State.Game.Phase == "Playing")
assert(TimeResetCount == 2 and FadeCount == 1)

-- 서로 다른 모드의 지형 확률·나무 수를 적용하고 큰 쪽 풀을 재사용한다.
local MapConfig = dofile("Assets/Scripts/ConfigManager.lua")
MapConfig.Map.Day = { RoadChance = 1, RailChance = 0, TreesPerRow = 0, TreeWeights = { 1 } }
MapConfig.Map.Night = { RoadChance = 0, RailChance = 0, TreesPerRow = 8,
    TreeWeights = { 0, 0, 0, 0, 0, 0, 0, 0, 1 } }
Base.dofile = function() return MapConfig end
Base.USceneComponent = { Cast = function(V) return V end }
Base.UBoxComponent = { Cast = function(V) return V end }
Base.ECollisionEnabled = { NoCollision = 0, QueryOnly = 1 }
Base.ECollisionChannel = { WorldStatic = 0, Pawn = 1 }
Base.ECollisionResponse = { Ignore = 0, Block = 1 }
local function MapComponent()
    local C = Component()
    function C:SetCollisionEnabled(V) self.Collision = V end
    function C:SetCollisionObjectType() end
    function C:SetCollisionResponseToAllChannels() end
    function C:SetCollisionResponseToChannel() end
    function C:SetBoxExtent() end
    function C:SetRelativeScale3D(V) self.Scale = V end
    function C:GetRelativeScale3D() return self.Scale or Vector(1, 1, 1) end
    function C:GetRelativeRotation() return self.Rotation or {} end
    function C:GetStaticMesh() return self.Mesh end
    return C
end
Components.MapFloor_1 = MapComponent()
for Index = 1, 5 do
    Components["MapTree_1_" .. Index] = MapComponent()
    Components["MapTreeHitbox_1_" .. Index] = MapComponent()
    Components["MapTreeHitbox_1_" .. Index].Name = "MapTreeHitbox_1_" .. Index
    Components["MapTreeMesh_1_" .. Index] = MapComponent()
end
Components.MapTreeMesh_1_1 = MapComponent()
Components.MapTreeMesh_1_1:SetStaticMesh("tree-template")
function Actor:AddComponent(_, Name)
    Components[Name] = MapComponent()
    Components[Name].Name = Name
    return Components[Name]
end
local Map = Load("World/MapGenerator.lua")
Base.dofile = nil
local MapLayout = { NightMode = false, HasMapPool = true, RowsBehind = 0, RowsAhead = 0,
    HalfWidth = 24, CellSize = 3 }
Map.Initialize(MapLayout)
local Rows, Trees = Map.GetPoolSize()
assert(Rows == 1 and Trees == 8)
for Index = 6, 8 do
    local Tree = assert(Components["MapTree_1_" .. Index])
    local Mesh = assert(Components["MapTreeMesh_1_" .. Index])
    assert(Mesh.Parent == Tree and Mesh.Mesh == "tree-template")
    assert(Mesh.Collision == Base.ECollisionEnabled.NoCollision)
end
Map.ResetGeneration(1)
local Id, Index, Kind = Map.GenerateNextRow(3, 3)
assert(Kind == "road" and Map.GetRows()[1].TreeCount == 0)
Map.RecycleRow(Id, false)
MapLayout.NightMode = true
Map.ResetGeneration(1)
Id, Index, Kind = Map.GenerateNextRow(3, 3)
assert(Kind == "grass" and Map.GetRows()[1].TreeCount == 8)
local Hitbox = Components.MapTreeHitbox_1_1
local HitMesh = Components.MapTreeMesh_1_1
HitMesh.Scale, HitMesh.Rotation = Vector(2, 3, 4), { Pitch = 5, Yaw = 10, Roll = 15 }
local TerrainSeedBeforeHit = Map.State.RandomState
Map.PlayTreeHit(Hitbox)
local Effect = Map.State.TreeEffects[HitMesh]
local PitchPhase, RollPhase = Effect.PitchPhase, Effect.RollPhase
assert(PitchPhase >= 0 and PitchPhase < 2 * math.pi and RollPhase >= 0 and RollPhase < 2 * math.pi)
assert(Map.State.RandomState == TerrainSeedBeforeHit)
Map.UpdateTreeHits(MapConfig.Map.HitSquash.Duration / 2)
assert(Effect.PitchPhase == PitchPhase and Effect.RollPhase == RollPhase)
assert(math.abs(HitMesh.Scale.X - 1.8) < 0.000001 and math.abs(HitMesh.Scale.Z - 3.6) < 0.000001)
assert(HitMesh.Rotation.Pitch ~= 5 or HitMesh.Rotation.Roll ~= 15)
Map.PlayTreeHit(Hitbox)
Map.UpdateTreeHits(MapConfig.Map.HitSquash.Duration)
assert(HitMesh.Scale.X == 2 and HitMesh.Scale.Z == 4 and HitMesh.Rotation.Roll == 15)
Map.PlayTreeHit(Hitbox)
Map.UpdateTreeHits(0.05)

Map.RecycleRow(Id, false)
assert(HitMesh.Scale.X == 2 and HitMesh.Rotation.Pitch == 5)
MapConfig.Map.Night.RailChance = 1
Map.ResetGeneration(1)
Id, Index, Kind = Map.GenerateNextRow(3, 3)
assert(Kind == "rail" and Map.GetRows()[1].TreeCount == 0)
-- 선택 조명·배경은 플레이어 카메라 하위에 생성하고 모드 색상과 표시 상태를 따른다.
local SelectionCamera = Component()
function SelectionCamera:GetWorldLocation() return self.Position end
function SelectionCamera:GetWorldRotation() return {} end
function SelectionCamera:SetWorldLocation(Value) self.Position = Value end
function SelectionCamera:SetWorldRotation(Value) self.Rotation = Value end
function SelectionCamera:SetIsOrthogonal() end
function SelectionCamera:SetOrthoWidth(Value) self.Width = Value end
function SelectionCamera:GetOrthoWidth() return self.Width or 12 end
function SelectionCamera:GetAspectRatio() return 2 end
function World:GetPlayerPawn() return Actor end
function Actor:AddComponent(_, Name)
    local C = MapComponent()
    function C:SetCastShadow(Value) self.CastShadow = Value end
    function C:SetUnlitColor(Value) self.Color = Value end
    Components[Name] = C
    return C
end
local SelectionView = Load("Camera/GameCamera.lua")
SelectionView.Initialize(SelectionCamera)
local Spot = assert(Components.CharacterSelectionSpotlight)
local Back = assert(Components.CharacterSelectionBackground)
assert(Spot.Parent == SelectionCamera and Back.Parent == SelectionCamera)
assert(not Spot.Visible and not Back.Visible and not Back.CastShadow)
SelectionView.ShowSelectionCamera(Vector(-208, 0, 8))
assert(Spot.Visible and Back.Visible and Back.Scale.Y / Back.Scale.Z == 2)
SelectionView.SetSelectionMode("Night")
assert(Back.Color[1] == Defaults.Selection.Background.Night.Color[1])
SelectionView.SetSelectionMode("Day")
assert(Back.Color[1] == Defaults.Selection.Background.Day.Color[1])
SelectionView.Refs.Camera = nil
SelectionView.ApplyGameplayCamera()
assert(not Spot.Visible and not Back.Visible)
print('PASS merge regression: death squash, train warning/audio settings, night mode: 200 car headlights, car speed/interval, train speed/wait, spotlight direction/visibility/reuse')
