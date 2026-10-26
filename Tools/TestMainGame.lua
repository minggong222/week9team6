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
    Camera.Rotation.Quaternion = function() return { GetForwardVector = function() return Vector(1, 0, 0) end,
        GetUpVector = function() return Vector(0, 0, 1) end } end
    function World:GetPlayerCamera() return Camera end
    local Base = setmetatable({ _G = Shared, print = function() end }, { __index = _G })
    Base.FVector, Base.FRotator = { new = Vector }, { new = function(P, Y, R) return { Pitch = P, Yaw = Y, Roll = R } end }
    Base.FTransform, Base.UClass = { new = function() return {} end }, { Find = function(N) return N end }
    Base.USceneComponent = { Cast = function(C) return C and not C.LuaKind and C or nil end }
    Base.UStaticMeshComponent = { Cast = function(C) return C and C.MeshKind and C or nil end }
    Base.ULuaComponent = { Cast = function(C) return C and C.LuaKind and C or nil end, StaticClass = function() return true end }
    Base.UPrimitiveComponent = { Cast = function() return nil end }
    Base.UBoxComponent = { Cast = function(C) return C and C.BoxKind and C or nil end }
    Base.ECollisionEnabled = { NoCollision = "NoCollision", QueryOnly = "QueryOnly" }
    Base.ECollisionChannel = { Pawn = "Pawn", WorldStatic = "WorldStatic" }
    Base.ECollisionResponse = { Ignore = "Ignore", Block = "Block" }
    Base.UTextRenderComponent = { Cast = function(C) return C and C.TextKind and C or nil end }
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
    end
    local LevelActor = { GetWorld = function() return World end, GetActorLocation = function() return Vector() end,
        FindComponentByName = function(_, Name) return Components[Name] end }
    function LevelActor:SetActorEnableCollision(V) World.LevelCollision = V end
    function LevelActor:AddComponent(Class, Name)
        assert(Class == "UBoxComponent")
        local C = Component(false); C.BoxKind = true; Components[Name] = C
        World.TreeShapes[#World.TreeShapes + 1] = C
        return C
    end
    local HUD = { TextKind = true, SetText = function(Self, T) Self.Text = T end, SetTextSize = function() end,
        SetVisibility = function(Self, V) Self.Visible = V end, SetWorldLocation = function() end, SetWorldRotation = function() end }
    local GameActor = { GetWorld = function() return World end, FindComponentByName = function(_, N) if N == "ScoreText" then return HUD end end }
    local LevelScript, GameScript
    if GameManagerFirst then
        GameScript = Script(GameActor, "Scripts/GameManager.lua"); GameScript:Invoke("BeginPlay", GameScript)
        LevelScript = Script(LevelActor, "Scripts/LevelManager.lua"); LevelScript:Invoke("BeginPlay", LevelScript)
    else
        LevelScript = Script(LevelActor, "Scripts/LevelManager.lua"); LevelScript:Invoke("BeginPlay", LevelScript)
        if WithGameManager then GameScript = Script(GameActor, "Scripts/GameManager.lua"); GameScript:Invoke("BeginPlay", GameScript) end
    end
    local Test = { Shared = Shared, Keys = Keys, Root = Root, Mesh = Mesh, Player = Player, PlayerScript = PlayerScript,
        Cars = Cars, Previews = Previews, Camera = Camera, Components = Components, World = World, HUD = HUD,
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
        assert(not Shared.CrossyLevel and not Shared.CrossyGameState)
    end
    if WithGameManager and not KeepSelecting then
        Keys.Enter = true; GameScript:Invoke("TickComponent", GameScript, 0); Keys.Enter = nil
    end
    Test.StartScaleZ = Mesh.Scale.Z
    return Test
end

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
    assert(S.Phase == "Dead" and T.HUD.Text:find("GAME OVER") and T.Sounds["Audio/Collision.wav"] == 1)
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
    assert(S.Phase == "Dead" and T.HUD.Text:find("GAME OVER"))
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
assert(State.Phase == "SelectingCharacter" and CharacterCount >= 24 and not Select.HUD.Visible)
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
assert(Select.Mesh.Asset == "Models/OBJ format/animal-" .. LastCharacter:lower() .. ".obj" and Select.Camera.Position.X == -8 and Select.Camera.Position.Y == 0)
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
local Directions = { { "Up", -3, 0 }, { "Down", 3, 0 }, { "Right", 0, -3 }, { "Left", 0, 3 } }
for _, D in ipairs(Directions) do
    PlaceNearTree(D[1], D[2], D[3])
    local Start, SoundsBefore = Vector(Trees.Root.Position), Trees.Sounds["Audio/Move.wav"] or 0
    Trees.Keys[D[1]] = true; Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.1); Trees.Keys[D[1]] = nil
    assert(Trees.Root.Position.X == Start.X and Trees.Root.Position.Y == Start.Y, "Walked through tree from " .. D[1])
    assert((Trees.Sounds["Audio/Move.wav"] or 0) == SoundsBefore and Trees.Shared.CrossyGameState.Phase == "Playing")
end
-- Keep the same tree position; moving only its collider vertically must unblock the hop.
PlaceNearTree("Up", -3, 0)
TreeBox:SetRelativeLocation(Vector(0, 0, 100))
Trees.Keys.Up = true; Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.1); Trees.Keys.Up = nil
assert(Trees.Root.Position.X == TreePosition.X, "Movement used a blocked-cell table instead of shape geometry")
TreeBox:SetRelativeLocation(Vector(0, 0, 1))
-- A collider enabled during a hop must cancel movement and return to the original grid center.
PlaceNearTree("Up", -3, 0)
TreeBox:SetCollisionEnabled("NoCollision")
Trees.Keys.Up = true; Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.04); Trees.Keys.Up = nil
assert(Trees.Root.Position.X > TreePosition.X - 3)
TreeBox:SetCollisionEnabled("QueryOnly")
Trees.PlayerScript:Invoke("TickComponent", Trees.PlayerScript, 0.06)
assert(Trees.Root.Position.X == TreePosition.X - 3 and Trees.Mesh.Position.Z == 0)
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
print("PASS tree shapes: four-way blocking without death/SFX, geometry-based checks, mid-hop cancellation, pooled collider reuse and cleanup")

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
