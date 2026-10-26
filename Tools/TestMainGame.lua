-- Run from the repository root: Source/ThirdParty/luajit/luajit.exe Tools/TestMainGame.lua
-- Exercises the actual gameplay scripts with engine API mocks; does not launch PIE.
local VectorMethods, VectorMeta = {}, {}
local function Vector(X, Y, Z)
    if type(X) == "table" then return setmetatable({ X = X.X, Y = X.Y, Z = X.Z }, VectorMeta) end
    return setmetatable({ X = X or 0, Y = Y or 0, Z = Z or 0 }, VectorMeta)
end
VectorMeta.__index = VectorMethods
VectorMeta.__add = function(A, B) return Vector(A.X + B.X, A.Y + B.Y, A.Z + B.Z) end
VectorMeta.__sub = function(A, B) return Vector(A.X - B.X, A.Y - B.Y, A.Z - B.Z) end
VectorMeta.__mul = function(A, B) return Vector(A.X * B, A.Y * B, A.Z * B) end
function VectorMethods:GetSafeNormal()
    local Length = math.sqrt(self.X * self.X + self.Y * self.Y + self.Z * self.Z)
    return Vector(self.X / Length, self.Y / Length, self.Z / Length)
end
local AssetBounds = {}
local function Rotation(Pitch, Yaw, Roll)
    local R = { Pitch = Pitch, Yaw = Yaw, Roll = Roll }
    function R:Quaternion()
        local P, Y = math.rad(self.Pitch), math.rad(self.Yaw)
        return { GetForwardVector = function() return Vector(math.cos(P) * math.cos(Y), math.cos(P) * math.sin(Y), -math.sin(P)) end,
            GetRightVector = function() return Vector(-math.sin(Y), math.cos(Y), 0) end,
            GetUpVector = function() return Vector(math.sin(P) * math.cos(Y), math.sin(P) * math.sin(Y), math.cos(P)) end }
    end
    return R
end
local function ReadBounds(Path)
    if AssetBounds[Path] then return AssetBounds[Path] end
    local Min, Max = Vector(math.huge, math.huge, math.huge), Vector(-math.huge, -math.huge, -math.huge)
    for Line in io.lines("Assets/" .. Path) do
        local X, Y, Z = Line:match("^v ([^ ]+) ([^ ]+) ([^ ]+)")
        if X then
            local Point = Vector(-tonumber(Z), tonumber(X), tonumber(Y))
            for _, Axis in ipairs({ "X", "Y", "Z" }) do
                Min[Axis], Max[Axis] = math.min(Min[Axis], Point[Axis]), math.max(Max[Axis], Point[Axis])
            end
        end
    end
    AssetBounds[Path] = { Min = Min, Max = Max }
    return AssetBounds[Path]
end

local function Setup(WithMap, WithGameManager, GameManagerFirst, KeepSelecting)
    local Shared, Keys, Cars, Components, Sounds, Previews = {}, {}, {}, {}, {}, {}
    local NextUUID = 0
    local World = { CollisionsEnabled = false, TreeShapes = {} }
    local function Component(IsMesh, Parent)
        local C = { MeshKind = IsMesh, Parent = Parent, Position = Vector(), Scale = Vector(1, 1, 1), Rotation = { Yaw = 0 } }
        function C:SetVisibility(Value) self.Visible = Value end
        function C:SetStaticMesh(Value) self.Asset = Value end
        function C:GetStaticMesh() return self.Asset end
        function C:SetRelativeLocation(Value) self.Position = Vector(Value) end
        function C:GetRelativeLocation() return Vector(self.Position) end
        function C:SetRelativeRotation(Value) self.Rotation = Value end
        function C:GetRelativeRotation() return self.Rotation end
        function C:SetRelativeScale3D(Value) self.Scale = Vector(Value) end
        function C:GetRelativeScale3D() return Vector(self.Scale) end
        function C:GetWorldScale3D()
            if not self.Parent then return Vector(self.Scale) end
            local P = self.Parent:GetWorldScale3D()
            return Vector(P.X * self.Scale.X, P.Y * self.Scale.Y, P.Z * self.Scale.Z)
        end
        function C:GetWorldLocation()
            if not self.Parent then return Vector(self.Position) end
            local A = self.Parent.Rotation.Yaw * math.pi / 180
            local S = self.Parent:GetWorldScale3D()
            return self.Parent:GetWorldLocation() + Vector(self.Position.X * S.X * math.cos(A) - self.Position.Y * S.Y * math.sin(A),
                self.Position.X * S.X * math.sin(A) + self.Position.Y * S.Y * math.cos(A), self.Position.Z * S.Z)
        end
        function C:GetWorldRotation() return self.Rotation end
        C.SetWorldLocation, C.SetWorldRotation = C.SetRelativeLocation, C.SetRelativeRotation
        function C:SetupAttachment(P) self.Parent = P end
        function C:SetBoxExtent(V) self.Extent = Vector(V) end
        function C:SetCollisionEnabled(V) self.Enabled = V end
        function C:SetCollisionObjectType(V) self.ObjectType = V end
        function C:SetCollisionResponseToAllChannels(V) self.DefaultResponse, self.Responses = V, {} end
        function C:SetCollisionResponseToChannel(K, V) self.Responses[K] = V end
        function C:SetLightColor(V) self.LightColor = V end
        function C:SetIntensity(V) self.Intensity = V end
        function C:SetAttenuationRadius(V) self.Radius = V end
        function C:SetCastShadows(V) self.CastShadows = V end
        function C:GetName() return self.Name or "" end
        function C:SetCastShadow(V) self.CastShadow = V end
        function C:GetAspectRatio() return 16 / 9 end
        function C:GetIsOrthogonal() return self.Orthogonal or false end
        function C:SetIsOrthogonal(V) self.Orthogonal = V end
        function C:GetOrthoWidth() return self.OrthoWidth or 12 end
        function C:SetOrthoWidth(V) self.OrthoWidth = V end
        function C:GetFieldOfView() return 60 end
        function C:CalcBounds()
            if self.Extent then
                local P, S = self:GetWorldLocation(), self:GetWorldScale3D()
                local E = Vector(self.Extent.X * S.X, self.Extent.Y * S.Y, self.Extent.Z * S.Z)
                return { Min = P - E, Max = P + E }
            end
            local B, Center = ReadBounds(assert(self.Asset)), self:GetWorldLocation()
            local Yaw = self.Rotation.Yaw + (self.Parent and self.Parent.Rotation.Yaw or 0)
            local Angle, Min, Max = Yaw * math.pi / 180, Vector(math.huge, math.huge, math.huge), Vector(-math.huge, -math.huge, -math.huge)
            for _, X in ipairs({ B.Min.X, B.Max.X }) do for _, Y in ipairs({ B.Min.Y, B.Max.Y }) do for _, Z in ipairs({ B.Min.Z, B.Max.Z }) do
                local SX, SY = X * self.Scale.X, Y * self.Scale.Y
                local Point = Center + Vector(SX * math.cos(Angle) - SY * math.sin(Angle), SX * math.sin(Angle) + SY * math.cos(Angle), Z * self.Scale.Z)
                for _, Axis in ipairs({ "X", "Y", "Z" }) do Min[Axis], Max[Axis] = math.min(Min[Axis], Point[Axis]), math.max(Max[Axis], Point[Axis]) end
            end end end
            return { Min = Min, Max = Max }
        end
        return C
    end
    local Root, Mesh = Component(false), Component(true)
    Mesh.Parent, Mesh.Asset = Root, "Models/OBJ format/animal-pig.obj"
    Mesh.Scale, Mesh.Rotation = Vector(1.369862914, 1.369862914, 1.369862914), { Pitch = 0, Yaw = 180, Roll = 0 }
    local Player = { Shapes = {} }
    function Player:GetActorLocation() return Root:GetWorldLocation() end
    function Player:GetRootComponent() return Root end
    function Player:SetActorEnableCollision(V) self.Collision = V end
    function Player:AddComponent(Class, Name)
        assert(Class == "UBoxComponent")
        local C = Component(false, Root); C.BoxKind = true
        self.Shapes[Name] = C
        return C
    end
    local function Intersects(A, B)
        for _, Axis in ipairs({ "X", "Y", "Z" }) do
            if A.Min[Axis] > B.Max[Axis] or A.Max[Axis] < B.Min[Axis] then return false end
        end
        return true
    end
    function Root:TryMoveWithBlocking(Delta)
        local Start = Vector(self.Position)
        local Shape = Player.Shapes.PlayerTreeHitbox
        assert(Shape, "Player tree collision shape was not initialized")
        local Before = Shape:CalcBounds()
        self.Position = Start + Delta
        for _, Tree in ipairs(World.TreeShapes) do
            if Player.Collision and World.LevelCollision and Tree.Enabled == "QueryOnly"
                and Shape.Responses[Tree.ObjectType] == "Block" and Tree.Responses[Shape.ObjectType] == "Block"
                and not Intersects(Before, Tree:CalcBounds()) and Intersects(Shape:CalcBounds(), Tree:CalcBounds()) then
                self.Position = Start; World.BlockedMoves = (World.BlockedMoves or 0) + 1
                return false
            end
        end
        return true
    end
    function Player:TryGetActorBounds() return World.CollisionsEnabled, Mesh:CalcBounds() end
    function World:GetPlayerPawn() return Player end
    local Camera = Component(false)
    Camera.Position = Vector(-8, 0, 10)
    Camera.Rotation = Rotation(40, 0, 0)
    function World:GetPlayerCamera() return Camera end
    local Base = setmetatable({ _G = Shared, print = function() end }, { __index = _G })
    Base.FVector, Base.FRotator = { new = Vector }, { new = Rotation }
    Base.FTransform, Base.UClass = { new = function() return {} end }, { Find = function(N) return N end }
    Base.USceneComponent = { Cast = function(C) return C and not C.LuaKind and C or nil end }
    Base.UStaticMeshComponent = { Cast = function(C) return C and C.MeshKind and C or nil end }
    Base.ULuaComponent = { Cast = function(C) return C and C.LuaKind and C or nil end, StaticClass = function() return true end }
    Base.UPrimitiveComponent = { Cast = function() return nil end }
    Base.UBoxComponent = { Cast = function(C) return C and C.BoxKind and C or nil end }
    Base.UPointLightComponent = { Cast = function(C) return C and C.LightKind and C or nil end }
    Base.FColor = { new = function(R, G, B, A) return { R = R, G = G, B = B, A = A } end }
    Base.ECollisionEnabled = { NoCollision = "NoCollision", QueryOnly = "QueryOnly" }
    Base.ECollisionChannel = { Pawn = "Pawn", WorldStatic = "WorldStatic" }
    Base.ECollisionResponse = { Ignore = "Ignore", Block = "Block" }
    Base.UTextRenderComponent = { Cast = function(C) return C and C.TextKind and C or nil end,
        StaticClass = function() return "UTextRenderComponent" end }
    Base.UAssetManager = { GetAssetByPath = function(_, P) ReadBounds(P); return P end }
    Base.Input = { IsKeyDown = function(K) return Keys[K] or false end, IsKeyPressed = function(K) return Keys[K] or false end }
    Base.EKeyCode = { W = "W", A = "A", S = "S", D = "D", R = "R", P = "P", K = "K",
        Up = "Up", Down = "Down", Left = "Left", Right = "Right", Enter = "Enter", Space = "Space" }
    Base.Audio = { PlayBGM = function() Sounds.BGM = true; return true end, StopBGM = function() Sounds.BGM = false end,
        PlaySFX = function(P) Sounds[P] = (Sounds[P] or 0) + 1; return true end }
    local function Script(Owner, Path)
        local S = { LuaKind = true, Path = Path }
        function S:InitializeLua() end
        function S:SetScriptPath(P) self.Path = P end
        function S:GetScriptPath() return self.Path end
        function S:LoadLua()
            self.Env = setmetatable({ Actor = Owner }, { __index = Base })
            setfenv(assert(loadfile("Assets/" .. self.Path)), self.Env)()
            return true
        end
        function S:Invoke(Name, ...) return assert(self.Env[Name], "Missing callback: " .. Name)(...) end
        if Path then S:LoadLua() end
        return S
    end
    local PlayerScript = Script(Player, "Scripts/Player.lua")
    function Player:GetComponents() return { Mesh, PlayerScript } end
    function Player:FindComponentByName(Name) if Name == "StaticMeshComponent" then return Mesh end; return self.Shapes[Name] end
    PlayerScript:Invoke("InitializeComponent", PlayerScript)
    function World:SpawnActor(_, Name, Pose)
        local CarRoot = Component(false); CarRoot.Position = Vector(Pose.Location)
        local CarMesh = Component(true, CarRoot)
        NextUUID = NextUUID + 1
        local A = { UUID = NextUUID, Root = CarRoot, Mesh = CarMesh }
        function A:GetUUID() return self.UUID end
        function A:GetWorld() return World end
        function A:GetActorLocation() return CarRoot:GetWorldLocation() end
        function A:GetRootComponent() return CarRoot end
        function A:FindComponentByName() return CarMesh end
        function A:SetActorEnableCollision(Value) self.Collision = Value end
        function A:TryGetActorBounds() return World.CollisionsEnabled, CarMesh:CalcBounds() end
        function A:AddComponent() self.Script = Script(self); return self.Script end
        function A:Destroy() self.Destroyed = true end
        if Name:find("^CharacterPreview_") then Previews[#Previews + 1] = A else Cars[#Cars + 1] = A end
        return A
    end
    if WithMap then
        for Index = 1, 41 do
            Components["MapFloor_" .. Index], Components["MapTrain_" .. Index] = Component(true), Component(true)
            for Tree = 1, 5 do Components["MapTree_" .. Index .. "_" .. Tree] = Component(false) end
        end
        for _, Name in ipairs({ "MapBackdrop", "MapBoundaryLeft", "MapBoundaryRight" }) do Components[Name] = Component(false) end
        -- An old editor preview that is outside the current signal layout must be hidden at startup.
        local Name = "RailSignal_1_99"
        local OldMesh, OldLight = Component(true), Component(false)
        OldMesh.Name, OldMesh.Visible = Name, true
        OldLight.Name, OldLight.LightKind, OldLight.Intensity, OldLight.Visible = Name .. "_Light", true, 6, true
        Components[Name], Components[Name .. "_Light"] = OldMesh, OldLight
    end
    local LevelActor = { GetWorld = function() return World end, GetActorLocation = function() return Vector() end,
        FindComponentByName = function(_, Name) return Components[Name] end }
    local LevelRoot = Component(false)
    function LevelActor:GetRootComponent() return LevelRoot end
    function LevelActor:GetComponents()
        local List = {}; for _, C in pairs(Components) do List[#List + 1] = C end
        return List
    end
    function LevelActor:SetActorEnableCollision(V) World.LevelCollision = V end
    function LevelActor:AddComponent(Class, Name)
        assert(Class == "UBoxComponent" or Class == "UStaticMeshComponent" or Class == "UPointLightComponent")
        local C = Component(Class == "UStaticMeshComponent")
        C.BoxKind, C.LightKind = Class == "UBoxComponent", Class == "UPointLightComponent"
        C.Name = Name
        Components[Name] = C
        if C.BoxKind then World.TreeShapes[#World.TreeShapes + 1] = C end
        return C
    end
    local HUD = Component(false)
    HUD.TextKind = true
    function HUD:SetText(T) self.Text = T end
    function HUD:GetText() return self.Text end
    function HUD:SetTextSize(V) self.TextSize = V end
    local UI = { ScoreText = HUD }
    local GameActor = { GetWorld = function() return World end, FindComponentByName = function(_, N) return UI[N] end }
    function GameActor:AddComponent(Class, Name)
        assert(Class == "UTextRenderComponent")
        local C = {}; for K, V in pairs(HUD) do C[K] = V end
        UI[Name] = C; return C
    end
    local LevelScript, GameScript
    if GameManagerFirst then
        GameScript = Script(GameActor, "Scripts/GameManager.lua"); GameScript:Invoke("BeginPlay", GameScript)
        LevelScript = Script(LevelActor, "Scripts/LevelManager.lua"); LevelScript:Invoke("BeginPlay", LevelScript)
    else
        LevelScript = Script(LevelActor, "Scripts/LevelManager.lua"); LevelScript:Invoke("BeginPlay", LevelScript)
        if WithGameManager then GameScript = Script(GameActor, "Scripts/GameManager.lua"); GameScript:Invoke("BeginPlay", GameScript) end
    end
    local Test = { Shared = Shared, Keys = Keys, Root = Root, Mesh = Mesh, Player = Player, PlayerScript = PlayerScript,
        Cars = Cars, Previews = Previews, Camera = Camera, Components = Components, World = World, HUD = HUD, UI = UI,
        Sounds = Sounds, LevelScript = LevelScript, GameScript = GameScript }
    function Test:Tick(DT)
        PlayerScript:Invoke("TickComponent", PlayerScript, DT)
        LevelScript:Invoke("TickComponent", LevelScript, DT)
        if GameScript then GameScript:Invoke("TickComponent", GameScript, DT) end
        for _, Car in ipairs(Cars) do Car.Script:Invoke("TickComponent", Car.Script, DT) end
    end
    function Test:Restart()
        Keys.R = true; self:Tick(0); Keys.R = nil
        -- GameManager가 요청한 점프/방향 초기화는 다음 Player Tick에서 완료된다.
        self:Tick(0)
        assert(not Shared.CrossyGameState.PlayerResetRequested, "Player reset request was not consumed")
    end
    function Test:Finish()
        LevelScript:Invoke("EndPlay", LevelScript)
        if GameScript then GameScript:Invoke("EndPlay", GameScript) end
        for _, Car in ipairs(Cars) do assert(Car.Destroyed) end
        for _, Preview in ipairs(Previews) do assert(Preview.Destroyed) end
        for _, Shape in ipairs(World.TreeShapes) do assert(Shape.Enabled == "NoCollision") end
        for Name, C in pairs(Components) do
            if Name:find("^RailSignal_") then
                assert(C.Visible == false, "Rail signal remained visible after EndPlay")
                if C.LightKind then assert(C.Intensity == 0) end
            end
        end
        assert(not Shared.CrossyLevel and not Shared.CrossyGameState)
    end
    if WithGameManager and not KeepSelecting then
        Keys.Enter = true; GameScript:Invoke("TickComponent", GameScript, 0); Keys.Enter = nil
    end
    Test.StartScaleZ = Mesh.Scale.Z
    return Test
end

-- 실제 메시 경계로 초기 배치와 반복 스폰의 차량 간 여유 거리를 확인합니다.
local Traffic = Setup(true, true, false)
local VehicleLengths = {}
for _, Car in ipairs(Traffic.Cars) do
    local B = ReadBounds(Car.Mesh.Asset)
    VehicleLengths[Car.Mesh.Asset:match("([^/]+)%.obj$")] = (B.Max.X - B.Min.X) * Car.Mesh.Scale.X
end
assert(VehicleLengths.firetruck > VehicleLengths.sedan * 1.25 and VehicleLengths["garbage-truck"] > VehicleLengths.sedan * 1.25,
    "Long trucks are absent or lost their original proportions")
local function CheckCarSpacing()
    local Lanes = {}
    for _, Car in ipairs(Traffic.Cars) do
        if Car.Script:Invoke("IsActive") then
            local X = Car:GetActorLocation().X
            Lanes[X] = Lanes[X] or {}
            Lanes[X][#Lanes[X] + 1] = Car.Mesh:CalcBounds()
        end
    end
    for _, Cars in pairs(Lanes) do
        table.sort(Cars, function(A, B) return A.Min.Y < B.Min.Y end)
        for Index = 2, #Cars do
            assert(Cars[Index].Min.Y - Cars[Index - 1].Max.Y >= 1.0 - 0.0001,
                "Cars overlap or violate the minimum gap")
        end
    end
end
for Seed = 1, 3 do
    CheckCarSpacing()
    for Frame = 1, 1800 do
        Traffic:Tick(Frame % 120 == 0 and 0.2 or 1 / 60)
        CheckCarSpacing()
    end
    Traffic:Restart()
end
Traffic:Finish()
print("PASS traffic spacing: initial placement, repeated spawning, both directions, frame delays and three maps")

local Warning = Setup(true, true, false, true)
local WarningLevel = Warning.Shared.CrossyLevel
local RailIndex = next(WarningLevel.RailLanes); assert(RailIndex)
local RailSlot = RailIndex + 11
local function Lane() return WarningLevel.RailLanes[RailIndex] end
local Signal = Warning.Components["RailSignal_" .. RailSlot .. "_1"]
local Light = Warning.Components["RailSignal_" .. RailSlot .. "_1_Light"]
assert(Signal and Signal.Visible and Light.Intensity == 0 and not Light.Visible and not Light.CastShadows)
assert(Light.LightColor.R == 255 and Light.LightColor.G == 24)
for I = 1, 5 do
    local M = Warning.Components["RailSignal_" .. RailSlot .. "_" .. I]
    assert(M and M.Position.Y == (I - 3) * 12 + 1.5 and math.abs(M.Position.X - (RailIndex * 3 - 1.4)) < 0.0001)
end
assert(not Warning.Components.RailSignal_1_99.Visible and Warning.Components.RailSignal_1_99_Light.Intensity == 0)
local InitialWait = Lane().SecondsUntilSpawn
Warning.LevelScript:Invoke("TickComponent", Warning.LevelScript, 1)
assert(Lane().SecondsUntilSpawn == InitialWait and Light.Intensity == 0, "Selection advanced the train warning clock")
Warning.Keys.Enter = true; Warning:Tick(0); Warning.Keys.Enter = nil
assert(InitialWait >= 2, "First train skipped the full warning interval")
Warning.LevelScript:Invoke("TickComponent", Warning.LevelScript, InitialWait - 2 + 0.01)
assert(Lane().Warning and not Lane().TrainActive and Lane().SignalOn and Light.Intensity > 0)
assert(Signal.Asset:find("signal%-on.obj"))
Warning.LevelScript:Invoke("TickComponent", Warning.LevelScript, 0.25)
assert(Lane().Warning and not Lane().TrainActive and not Lane().SignalOn and Light.Intensity == 0)
assert(Signal.Asset:find("signal%-off.obj"))
Warning.LevelScript:Invoke("TickComponent", Warning.LevelScript, 0.25)
assert(Lane().SignalOn and Light.Intensity > 0)
Warning.LevelScript:Invoke("TickComponent", Warning.LevelScript, Lane().SecondsUntilSpawn + 0.01)
assert(Lane().TrainActive and Lane().Warning and Lane().SecondsUntilSpawn == 0)
Warning.LevelScript:Invoke("TickComponent", Warning.LevelScript, 10)
assert(not Lane().TrainActive and not Lane().Warning and Light.Intensity == 0 and Signal.Visible)
assert(Lane().SecondsUntilSpawn >= 3 and Lane().SecondsUntilSpawn <= 6)
Warning.LevelScript:Invoke("TickComponent", Warning.LevelScript, Lane().SecondsUntilSpawn - 2 - 0.05)
assert(not Lane().Warning and Light.Intensity == 0)
Warning.LevelScript:Invoke("TickComponent", Warning.LevelScript, 0.06)
assert(Lane().Warning and Light.Intensity > 0, "Repeated train did not warn before spawning")
Warning.Root.Position.X = (RailIndex + 11) * 3
Warning.LevelScript:Invoke("TickComponent", Warning.LevelScript, 0)
for Name, C in pairs(Warning.Components) do
    if Name:find("^RailSignal_%d+_%d+$") and C.Visible then
        local Index = math.floor((C.Position.X + 1.4) / 3 + 0.5)
        assert(WarningLevel.RailLanes[Index], "Recycled signal remained on a non-rail row")
    end
end
Warning:Restart(); Warning:Finish()
print("PASS long trucks and rail signals: spacing, 2s warning, 0.25s blinking, red lights, full passage, repeated warning, selection freeze and recycled-slot cleanup")

-- Blink lifetime is a timer, independent of train position/direction and speed.
local TimedWarning, DirectionsSeen = Setup(true, true, false), {}
for Attempt = 1, 10 do
    if DirectionsSeen[true] and DirectionsSeen[false] then break end
    if Attempt > 1 then TimedWarning:Restart() end
    local Index, Lane
    for I, L in pairs(TimedWarning.Shared.CrossyLevel.RailLanes) do
        if not DirectionsSeen[L.Direction] then Index, Lane = I, L; break end
    end
    if Lane then
        TimedWarning.Shared.CrossyLevel.SetTrainSpeed(40, Index)
        Lane = TimedWarning.Shared.CrossyLevel.RailLanes[Index]
        local Slot = Index + 11
        local function Advance(DT) TimedWarning.LevelScript:Invoke("TickComponent", TimedWarning.LevelScript, DT) end
        Advance(Lane.SecondsUntilSpawn - 2 + 0.01)
        assert(Lane.Warning and Lane.SignalOn and not Lane.TrainActive)
        Advance(Lane.SecondsUntilSpawn + 0.01)
        Advance(0.5) -- 2.51 seconds since warning started: still inside the 3-second lifetime.
        assert(Lane.TrainActive and Lane.Warning and Lane.SignalOn)
        Advance(0.5) -- 3.01 seconds: the train is still active, but every signal must be off.
        assert(Lane.TrainActive and not Lane.Warning and not Lane.SignalOn)
        for I = 1, 5 do
            local M = TimedWarning.Components["RailSignal_" .. Slot .. "_" .. I]
            local L = TimedWarning.Components["RailSignal_" .. Slot .. "_" .. I .. "_Light"]
            assert(L.Intensity == 0 and not L.Visible and M.Asset:find("signal%-off.obj"), "Blink lifetime did not switch every signal off")
        end
        Advance(0.5)
        assert(Lane.TrainActive and not Lane.Warning and not Lane.SignalOn, "Expired warning relit while the train was active")
        Advance(10)
        assert(not Lane.TrainActive)
        Advance(Lane.SecondsUntilSpawn - 2 + 0.01)
        assert(Lane.Warning and Lane.SignalOn, "Next train did not reset the blink lifetime")
        DirectionsSeen[Lane.Direction] = true
    end
end
assert(DirectionsSeen[true] and DirectionsSeen[false], "Both train directions were not covered")
TimedWarning:Finish()
print("PASS signal timer: 3-second total lifetime, simultaneous switch-off, independent of direction/speed, no relighting until next train")

for _, GameFirst in ipairs({ false, true }) do
    local T = Setup(true, true, GameFirst)
    local S, Level = T.Shared.CrossyGameState, T.Shared.CrossyLevel
    assert(#T.Cars == 100 and Level.ActiveRows == 41 and S.ManagedByGameManager and T.Sounds.BGM)
    local Models = {}
    for Row = 1, 500 do
        T.Root.Position.X = Row * 3; T:Tick(0.03)
        assert(Level.ActiveRows == 41 and Level.MinRow == Row - 10 and Level.MaxRow == Row + 30)
        assert(S.Score == Row and S.Phase == "Playing", "Endless mode cleared or score is not measured in rows")
        for _, Lane in pairs(Level.RailLanes) do Models[Lane.TrainType] = true end
        for _, Car in ipairs(T.Cars) do
            if Car.Script:Invoke("IsActive") then
                local Index = math.floor(Car:GetActorLocation().X / 3 + 0.5)
                assert(Level.RoadLanes[Index], "A car survived its recycled road or spawned on grass/rail")
                local Bounds = Car.Mesh:CalcBounds()
                assert(math.abs(Bounds.Max.X - Bounds.Min.X - 2) < 0.0001, "Car width does not fit a single row")
            end
        end
    end
    assert(Models.bullet and Models.city and Models.freight and #T.Cars == 100)
    T.Root.Position.X = (500 - 6) * 3; T:Tick(0); assert(S.Phase == "Playing")
    T.Root.Position.X = T.Root.Position.X - 0.1; T:Tick(0)
    assert(S.Phase == "Dead" and T.UI.HUDTitle.Text == "GAME OVER" and T.Sounds["Audio/Collision.wav"] == 1)
    assert(math.abs(T.Mesh.Scale.Z - T.StartScaleZ * 0.25) < 0.0001)
    T:Restart(); assert(S.Phase == "Playing" and S.Score == 0 and Level.HighestRow == 0 and T.Root.Position.X == 0)
    assert(math.abs(T.Mesh.Scale.Z - T.StartScaleZ) < 0.0001 and #T.Cars == 100)
    local Rail = next(Level.RailLanes); assert(Rail)
    Level.SetTrainSpeed(80, Rail); assert(Level.RailLanes[Rail].Speed == 80)
    Level.SetTrainSpeed(50); for _, Lane in pairs(Level.RailLanes) do assert(Lane.Speed == 50) end
    T.Root.Position.X = Rail * 3; T.World.CollisionsEnabled = true; T:Tick(20)
    assert(S.Phase == "Dead", "Swept train collision did not reach GameManager")
    T.World.CollisionsEnabled = false; T:Restart()
    -- A car collision must use the same squash/audio/HUD path.
    local Car
    for _, C in ipairs(T.Cars) do if C.Script:Invoke("IsActive") then Car = C; break end end
    assert(Car); T.Root.Position = Car:GetActorLocation(); T.World.CollisionsEnabled = true
    Car.Script:Invoke("TickComponent", Car.Script, 0)
    assert(S.Phase == "Dead" and T.UI.HUDTitle.Text == "GAME OVER")
    T.World.CollisionsEnabled = false; T:Restart()
    -- Kill in the middle of a jump; no remaining interpolation may move the pawn.
    T.Keys.W = true; T.PlayerScript:Invoke("TickComponent", T.PlayerScript, 0.04)
    local X = T.Root.Position.X; assert(X > 0 and X < 3)
    S.GameOver(); T.Keys.W = nil; T.PlayerScript:Invoke("TickComponent", T.PlayerScript, 0.2)
    assert(T.Root.Position.X == X, "Dead player continued jumping")
    T:Restart(); T:Finish()
    print("PASS main scene: 500 rows, 100 cars, row recycling, three trains, 10/6 boundary, shared death/HUD/audio, reset, jump cancellation; GM-first=" .. tostring(GameFirst))
end
local Preview = Setup(true, false, false)
assert(#Preview.Cars == 0 and Preview.Shared.CrossyLevel.ActiveRows == 41)
Preview.Root.Position.X = 30; Preview:Tick(0); Preview:Restart(); Preview:Finish()
local Static = Setup(false, true, false)
assert(#Static.Cars == 100 and not Static.Shared.CrossyLevel.IsEndless)
Static.Root.Position.X = 9; Static:Tick(0); assert(Static.Shared.CrossyGameState.Score == 3)
Static.Shared.CrossyGameState.GameOver(); Static:Restart(); Static:Finish()
print("PASS existing maptest without cars and existing fixed-map traffic")

local Select = Setup(true, true, false, true)
local State = Select.Shared.CrossyGameState
local CharacterCount = #Select.Previews
local InitialPreviewY = -(CharacterCount - 1) * 5 / 2
assert(State.Phase == "SelectingCharacter" and CharacterCount >= 24 and Select.HUD.Visible and Select.HUD.Text:find("BEAVER"))
assert(Select.Camera.Position.X == -208 and State.PreviewCharacter == "Beaver")
local CarPositions = {}
for I, Car in ipairs(Select.Cars) do CarPositions[I] = Car:GetActorLocation() end
Select.Keys.Up = true; Select:Tick(0.1); Select.Keys.Up = nil
assert(Select.Root.Position.X == 0, "Player moved during selection")
for I, Car in ipairs(Select.Cars) do assert(Car:GetActorLocation().Y == CarPositions[I].Y, "Traffic moved during selection") end
Select.Keys.Left = true; Select:Tick(0.1); Select.Keys.Left = nil
local LastCharacter = State.PreviewCharacter
assert(Select.Camera.Position.Y > InitialPreviewY, "Selection wrap/camera failed")
assert(Select.Previews[CharacterCount].Mesh.Asset == "Models/OBJ format/animal-" .. LastCharacter:lower() .. ".obj")
for _, Preview in ipairs(Select.Previews) do
    local B = Preview.Mesh:CalcBounds()
    assert(math.max(B.Max.X - B.Min.X, B.Max.Y - B.Min.Y) <= 2.4001 and math.abs(B.Min.Z) < 0.0001)
end
Select.Keys.Space = true; Select:Tick(0); Select.Keys.Space = nil
assert(State.Phase == "Playing" and State.SelectedCharacter == LastCharacter and Select.HUD.Visible)
assert(Select.Mesh.Asset == "Models/OBJ format/animal-" .. LastCharacter:lower() .. ".obj" and Select.Camera.Orthogonal
    and Select.Camera.OrthoWidth == 36 and Select.Camera.Rotation.Yaw == -35)
for _, Preview in ipairs(Select.Previews) do assert(Preview.Root.Visible == false) end
local B, ScaleZ = Select.Mesh:CalcBounds(), Select.Mesh.Scale.Z
assert(math.abs(math.max(B.Max.X - B.Min.X, B.Max.Y - B.Min.Y) - 2) < 0.0001 and math.abs(B.Min.Z) < 0.0001)
Select:Restart()
assert(State.SelectedCharacter == LastCharacter and Select.Mesh.Scale.Z == ScaleZ and State.RestartCount == 1)
-- Arrow movement remains one hop per press, including when a key is held across StopMovement.
Select.Keys.Up = true; Select.PlayerScript:Invoke("TickComponent", Select.PlayerScript, 0.1)
assert(Select.Root.Position.X == 3)
Select.PlayerScript:Invoke("TickComponent", Select.PlayerScript, 0.1); assert(Select.Root.Position.X == 3)
Select.PlayerScript:Invoke("StopMovement"); Select.PlayerScript:Invoke("TickComponent", Select.PlayerScript, 0.1)
assert(Select.Root.Position.X == 3)
Select.Keys.Up = nil; Select.PlayerScript:Invoke("TickComponent", Select.PlayerScript, 0)
Select.Keys.Up = true; Select.PlayerScript:Invoke("TickComponent", Select.PlayerScript, 0.1); Select.Keys.Up = nil
assert(Select.Root.Position.X == 6)
Select:Finish()
print("PASS selection: " .. CharacterCount .. " normalized previews, wrap, camera restore, confirm/score HUD, selected character retained on R, arrows and held-key cancellation")

local Trees = Setup(true, true, false)
local TreeBox
for _, Shape in ipairs(Trees.World.TreeShapes) do
    assert((Shape.Enabled == "QueryOnly") == (Shape.Parent.Visible == true), "Hidden tree still blocks")
    if Shape.Enabled == "QueryOnly" then TreeBox = TreeBox or Shape end
end
assert(TreeBox and #Trees.World.TreeShapes == 205)
local TreePosition = TreeBox.Parent:GetWorldLocation()
local function PlaceNearTree(Key, OffsetX, OffsetY)
    Trees.Root.Position = Vector(TreePosition.X + OffsetX, TreePosition.Y + OffsetY, 0)
    Trees.Keys[Key] = nil; Trees.PlayerScript:Invoke("StopMovement")
end
local Directions = { { "Up", -3, 0, 180 }, { "Down", 3, 0, 0 }, { "Right", 0, -3, -90 }, { "Left", 0, 3, 90 } }
for _, D in ipairs(Directions) do
    PlaceNearTree(D[1], D[2], D[3])
    local Start, SoundsBefore = Vector(Trees.Root.Position), Trees.Sounds["Audio/Move.wav"] or 0
    local function DistanceFromStart()
        return math.abs(Trees.Root.Position.X - Start.X) + math.abs(Trees.Root.Position.Y - Start.Y)
    end
    local HitsBefore = Trees.World.BlockedMoves or 0
    Trees.Keys[D[1]] = true; Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.04)
    assert(Trees.Mesh.Rotation.Yaw == D[4], "Player did not face the tree before impact")
    assert(DistanceFromStart() > 0 and DistanceFromStart() < 3 and Trees.Mesh.Position.Z > 0, "Did not hop toward tree")
    Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.02)
    local HitDistance, HitHeight = DistanceFromStart(), Trees.Mesh.Position.Z
    assert((Trees.World.BlockedMoves or 0) > HitsBefore and HitDistance > 0 and HitHeight > 0, "No visible contact with tree")
    Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.04)
    assert(DistanceFromStart() > 0 and DistanceFromStart() < HitDistance and Trees.Mesh.Position.Z < HitHeight, "Return teleported or did not descend")
    Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.2)
    assert(Trees.Root.Position.X == Start.X and Trees.Root.Position.Y == Start.Y and Trees.Mesh.Position.Z == 0, "Did not return to original grid center")
    Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.2); Trees.Keys[D[1]] = nil
    assert(DistanceFromStart() == 0, "Held input started another bounce")
    assert((Trees.Sounds["Audio/Move.wav"] or 0) == SoundsBefore + 1 and Trees.Shared.CrossyGameState.Phase == "Playing")
end
-- Keep the same tree position; moving only its collider vertically must unblock the hop.
PlaceNearTree("Up", -3, 0)
TreeBox:SetRelativeLocation(Vector(0, 0, 100))
Trees.Keys.Up = true; Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.1); Trees.Keys.Up = nil
assert(Trees.Root.Position.X == TreePosition.X, "Movement used a blocked-cell table instead of shape geometry")
TreeBox:SetRelativeLocation(Vector(0, 0, 1))
-- A collider enabled during a hop starts a visible return instead of teleporting.
PlaceNearTree("Up", -3, 0)
TreeBox:SetCollisionEnabled("NoCollision")
Trees.Keys.Up = true; Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.04); Trees.Keys.Up = nil
assert(Trees.Root.Position.X > TreePosition.X - 3)
TreeBox:SetCollisionEnabled("QueryOnly")
Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.06)
assert(Trees.Root.Position.X > TreePosition.X - 3 and Trees.Root.Position.X < TreePosition.X)
Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.2)
assert(Trees.Root.Position.X == TreePosition.X - 3 and Trees.Mesh.Position.Z == 0)
-- Death/StopMovement must cancel an in-progress return as well as an outgoing hop.
PlaceNearTree("Up", -3, 0)
Trees.Keys.Up = true; Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.07); Trees.Keys.Up = nil
local DeathX = Trees.Root.Position.X
assert(DeathX > TreePosition.X - 3)
Trees.Shared.CrossyGameState.GameOver(); Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.2)
assert(Trees.Root.Position.X == DeathX and Trees.Mesh.Position.Z == 0, "Dead player continued bouncing")
Trees:Restart()
-- Recycling and R reuse the existing shapes; hidden/unused slots never retain collision.
Trees.Root.Position = Vector(200, 0, 0); Trees.LevelScript:Invoke("TickComponent", Trees.LevelScript, 0)
assert(#Trees.World.TreeShapes == 205)
local ActiveTrees = 0
for _, Shape in ipairs(Trees.World.TreeShapes) do
    assert((Shape.Enabled == "QueryOnly") == (Shape.Parent.Visible == true), "Recycled tree still blocks")
    if Shape.Enabled == "QueryOnly" then ActiveTrees = ActiveTrees + 1 end
end
assert(ActiveTrees == Trees.Shared.CrossyLevel.ActiveTrees)
Trees:Restart(); assert(#Trees.World.TreeShapes == 205); Trees:Finish()
print("PASS tree shapes: visible hop/contact/return in four directions, held-input guard, geometry checks, mid-hop activation, death cancels return, pooled collider reuse")

-- Regression: every selectable index must complete confirmation and remain playable after R.
for _, ConfirmKey in ipairs({ "Enter", "Space" }) do
for CharacterIndex = 1, CharacterCount do
    local T = Setup(true, true, CharacterIndex % 2 == 0, true)
    local S = T.Shared.CrossyGameState
    for _ = 2, CharacterIndex do
        T.Keys.Right = true; T.GameScript:Invoke("TickComponent", T.GameScript, 0); T.Keys.Right = nil
    end
    local Expected = S.PreviewCharacter
    T.Keys[ConfirmKey] = true; T:Tick(0); T.Keys[ConfirmKey] = nil
    assert(S.Phase == "Playing" and S.SelectedCharacter == Expected and S.SelectedCharacterIndex == CharacterIndex,
        "Character " .. CharacterIndex .. " could not leave selection")
    assert(T.Mesh.Asset == "Models/OBJ format/animal-" .. Expected:lower() .. ".obj")
    T.Keys.Up = true; T.PlayerScript:Invoke("TickComponent", T.PlayerScript, 0.1); T.Keys.Up = nil
    T:Tick(0)
    assert(T.Root.Position.X == 3 and S.Score == 1, "Character " .. CharacterIndex .. " could not move/score")
    T:Restart()
    assert(S.Phase == "Playing" and S.SelectedCharacter == Expected and T.Root.Position.X == 0)
    T:Finish()
end
end
print("PASS all " .. CharacterCount .. " indices with Enter and Space: selection -> confirm -> movement/score -> restart; both manager initialization orders")
