-- 지형·나무·벽 슬롯만 소유. 차량·기차 상태는 각 시스템에서 별도 보관
local AllConfig = dofile("Assets/Scripts/ConfigManager.lua")

local Config = AllConfig.Map
local RiverConfig = AllConfig.River
State = { Rows = {}, RandomState = 1, LastRow = 0, SafeColumn = 0, ReusedRows = 0 , RiverRowsRemaining = 0, ForceGrassAfterRiver = false, LastKind = nil }
Refs = {}
State.TreeMeshesByHitbox, State.TreeEffects = {}, {}

local function RestoreTreeHit(Mesh)
    local Effect = State.TreeEffects[Mesh]
    if not Effect then return end
    Mesh:SetRelativeScale3D(Effect.Scale)
    Mesh:SetRelativeRotation(Effect.Rotation)
    State.TreeEffects[Mesh] = nil
end

function PlayTreeHit(BlockingComponent)
    local Mesh = State.TreeMeshesByHitbox[BlockingComponent:GetName()]
    if not Mesh then return end
    -- 연속 충돌에서도 원래 자세를 기준으로 다시 시작한다.
    RestoreTreeHit(Mesh)
    State.TreeEffects[Mesh] = {
        Elapsed = 0, Scale = Mesh:GetRelativeScale3D(), Rotation = Mesh:GetRelativeRotation(),
        -- 지형 생성 난수와 분리해 피격 횟수가 이후 맵 배치에 영향을 주지 않도록 한다.
        PitchPhase = math.random() * 2 * math.pi,
        RollPhase = math.random() * 2 * math.pi
    }
end

function UpdateTreeHits(DeltaSeconds)
    local Settings = Config.HitSquash
    for Mesh, Effect in pairs(State.TreeEffects) do
        Effect.Elapsed = Effect.Elapsed + DeltaSeconds
        local Alpha = math.min(1, Effect.Elapsed / Settings.Duration)
        if Alpha >= 1 then
            RestoreTreeHit(Mesh)
        else
            local Scale = 1 - (1 - Settings.Scale) * math.sin(math.pi * Alpha)
            local Angle = Settings.ShakeAngle * math.sin(math.pi * Alpha)
            local Phase = Effect.Elapsed * Settings.ShakeFrequency * 2 * math.pi
            local Base, Rotation = Effect.Scale, Effect.Rotation
            -- 메시만 변형해 충돌 박스와 안전 경로는 흔들리지 않게 한다.
            Mesh:SetRelativeScale3D(FVector.new(Base.X * Scale, Base.Y * Scale, Base.Z * Scale))
            Mesh:SetRelativeRotation(FRotator.new(Rotation.Pitch + math.sin(Phase + Effect.PitchPhase) * Angle,
                Rotation.Yaw, Rotation.Roll + math.sin(Phase * 1.3 + Effect.RollPhase) * Angle))
        end
    end
end

local function GetSettings()
    return Refs.Layout.NightMode and Config.Night or Config.Day
end

-- 두 모드 중 큰 용량으로 확보해 모드를 바꿔도 같은 풀을 재사용한다.
local TreePoolSize = math.max(Config.Day.TreesPerRow, Config.Night.TreesPerRow)

function BeginPlay(Self)
    Self:SetComponentTickEnabled(false)
end

-- 지형 전용 난수 스트림 사용. 교통 난수는 LevelManager에서 별도 관리
local function TerrainRandom()
    State.RandomState = State.RandomState * 48271 % 2147483647
    return State.RandomState / 2147483647
end

local function TerrainInteger(Min, Max)
    return Min + math.floor(TerrainRandom() * (Max - Min + 1))
end


local function PickRowType(Index)
    if Index < 3 then
        return "grass"
    end

    if State.RiverRowsRemaining > 0 then
        State.RiverRowsRemaining =
            State.RiverRowsRemaining - 1

        return "river"
    end

    if State.ForceGrassAfterRiver then
        State.ForceGrassAfterRiver = false
        return "grass"
    end

    local Settings = GetSettings()
    local RiverChance = Settings.RiverChance or 0.0
    local Choice = TerrainRandom()

    if State.LastKind == "grass"
        and Choice < RiverChance then

        local Count = TerrainInteger(
            (Refs.Layout.NightMode and RiverConfig.Night or RiverConfig.Day).MinRows,
            (Refs.Layout.NightMode and RiverConfig.Night or RiverConfig.Day).MaxRows
        )

        State.RiverRowsRemaining = Count - 1
        State.ForceGrassAfterRiver = true

        return "river"
    end

    if Choice < RiverChance then
        return "grass"
    end

    local RemainingChance = 1.0 - RiverChance
    Choice = (Choice - RiverChance) / RemainingChance

    local RailThreshold =
        Settings.RailChance / RemainingChance

    local RoadThreshold =
        (Settings.RailChance + Settings.RoadChance)
        / RemainingChance

    if Choice < RailThreshold then
        return "rail"
    end

    if Choice < RoadThreshold then
        return "road"
    end

    return "grass"
end


local function PickTreeCount()
    local Settings = GetSettings()
    local Total = 0
    for _, Weight in ipairs(Settings.TreeWeights) do Total = Total + Weight end
    local Choice = TerrainRandom() * Total
    for Index, Weight in ipairs(Settings.TreeWeights) do
        Choice = Choice - Weight
        if Choice < 0 then return Index - 1 end
    end
    return #Settings.TreeWeights - 1
end

local function GenerateMapRow(Row, Index, Kind)
    Row.Index, Row.Kind, Row.Active, Row.TreeCount = Index, Kind, true, 0
    if not Row.Floor then return end
    local FloorMeshes = {grass = Refs.GrassMesh, road = Refs.RoadMesh, rail = Refs.RailMesh, river = Refs.WaterMesh}
    if Kind == "river" and RiverConfig.WaterMeshMode == "Tile" then Row.Floor:SetVisibility(false, true)
    else
        local Mesh = assert(FloorMeshes[Kind], "Missing floor mesh for kind: " .. tostring(Kind))
        Row.Floor:SetStaticMesh(Mesh)
        Row.Floor:SetCastShadow(Kind ~= "river")
        Row.Floor:SetRelativeLocation(FVector.new(Index * Refs.Layout.CellSize, 0, Kind == "river" and RiverConfig.WaterZ or 0))
        if Kind == "river" then Row.Floor:SetRelativeScale3D(FVector.new(RiverConfig.WaterRowScale.X, RiverConfig.WaterRowScale.Y, RiverConfig.WaterRowScale.Z))
        else Row.Floor:SetRelativeScale3D(FVector.new(1, 1, 1)) end
        Row.Floor:SetVisibility(Kind ~= "grass", true)
    end

    -- 해당 행이 강물일 때만 물 타일을 표시한다.
    for _, Tile in ipairs(Row.WaterTiles) do
        if Kind == "river" and RiverConfig.WaterMeshMode == "Tile" then
            Tile.Component:SetRelativeLocation(FVector.new(Index * Refs.Layout.CellSize, Tile.Column * Refs.Layout.CellSize, RiverConfig.WaterZ))
            Tile.Component:SetCastShadow(false)
            Tile.Component:SetVisibility(true, true)
        else
            Tile.Component:SetVisibility(false, true)
        end
    end

    -- 바닥 끝(마지막 이동 칸의 바깥쪽)부터 벽 배치. 터널은 행마다 독립 배치
    for Side, Wall in ipairs(Row.Walls) do
        local Sign = Side == 1 and -1 or 1
        Wall:SetStaticMesh((Kind == "grass" or Kind == "river") and Refs.DirtWallMesh or Refs.TunnelMesh)
        Wall:SetRelativeLocation(FVector.new(Index * Refs.Layout.CellSize, Sign * (Refs.Layout.HalfWidth + Refs.Layout.CellSize / 2), 0))
        Wall:SetRelativeRotation(FRotator.new(0, Sign == 1 and 0 or 180, 0))
        Wall:SetVisibility(true, true)
    end

    for _, Hitbox in ipairs(Row.TreeHitboxes) do Hitbox:SetCollisionEnabled(ECollisionEnabled.NoCollision) end
    for _, Tree in ipairs(Row.Trees) do Tree:SetVisibility(false, true) end
    if Kind ~= "grass" then return end
    if Index < 3 then return end
    local HalfColumns = math.floor(Refs.Layout.HalfWidth / Refs.Layout.CellSize)
    local RouteLimit = math.max(0, HalfColumns - 2)
    -- 이전 행과 다음 행의 안전 경로 사이를 나무 배치에서 제외
    local Next = math.max(-RouteLimit, math.min(RouteLimit, State.SafeColumn + TerrainInteger(-1, 1)))
    local Columns = {}
    for Column = -HalfColumns, HalfColumns do
        if Column < math.min(State.SafeColumn, Next) or Column > math.max(State.SafeColumn, Next) then Columns[#Columns + 1] = Column end
    end
    State.SafeColumn = Next
    for Index = #Columns, 2, -1 do
        local Other = TerrainInteger(1, Index)
        Columns[Index], Columns[Other] = Columns[Other], Columns[Index]
    end
    Row.TreeCount = math.min(PickTreeCount(), #Columns)
    for Index = 1, Row.TreeCount do
        local Tree = Row.Trees[Index]
        Tree:SetRelativeLocation(FVector.new(Row.Index * Refs.Layout.CellSize, Columns[Index] * Refs.Layout.CellSize, 0))
        Tree:SetRelativeRotation(FRotator.new(0, TerrainInteger(0, 3) * 90, 0))
        local Scale = 0.9 + TerrainRandom() * 0.1
        Tree:SetRelativeScale3D(FVector.new(Scale, Scale, Scale))
        Tree:SetVisibility(true, true)
        Row.TreeHitboxes[Index]:SetCollisionEnabled(ECollisionEnabled.QueryOnly)
    end
end

local function RecycleMapRow(Row)
    Row.Active = false

    for _, Hitbox in ipairs(Row.TreeHitboxes or {}) do
        local Mesh = State.TreeMeshesByHitbox[Hitbox:GetName()]
        if Mesh then RestoreTreeHit(Mesh) end
        Hitbox:SetCollisionEnabled(ECollisionEnabled.NoCollision)
    end
    if Row.Floor then
        Row.Floor:SetVisibility(false, true)
        for _, Wall in ipairs(Row.Walls) do Wall:SetVisibility(false, true) end
        for _, Tree in ipairs(Row.Trees) do Tree:SetVisibility(false, true) end
        for _, Tile in ipairs(Row.WaterTiles or {}) do Tile.Component:SetVisibility(false, true) end
    end
end

-- 저장된 씬보다 나무 수를 늘려도 기존 메시 배치를 복사해 슬롯을 보충한다.
local function GetOrCreateTree(Slot, Index)
    local Name = "MapTree_" .. Slot .. "_" .. Index
    local Tree = USceneComponent.Cast(Actor:FindComponentByName(Name))
    if Tree then return Tree end

    local Template = UStaticMeshComponent.Cast(Actor:FindComponentByName("MapTreeMesh_" .. Slot .. "_1"))
    assert(Template, "Missing tree mesh template for slot " .. Slot)
    Tree = USceneComponent.Cast(Actor:AddComponent(UClass.Find("USceneComponent"), Name))
    local Mesh = UStaticMeshComponent.Cast(Actor:AddComponent(UClass.Find("UStaticMeshComponent"),
        "MapTreeMesh_" .. Slot .. "_" .. Index))
    assert(Tree and Mesh, "Could not create tree slot")
    Tree:SetupAttachment(Actor:GetRootComponent())
    Mesh:SetupAttachment(Tree)
    Mesh:SetStaticMesh(Template:GetStaticMesh())
    Mesh:SetRelativeLocation(Template:GetRelativeLocation())
    Mesh:SetRelativeRotation(Template:GetRelativeRotation())
    Mesh:SetRelativeScale3D(Template:GetRelativeScale3D())
    Mesh:SetCollisionEnabled(ECollisionEnabled.NoCollision)
    Tree:SetVisibility(false, true)
    return Tree
end

local function CaptureMapPool()
    if not State.HasMapPool then return end
    Refs.GrassMesh = UAssetManager:GetAssetByPath("Models/Crossy/RoadTiles/kenney-grass-row.obj")
    Refs.RoadMesh = UAssetManager:GetAssetByPath("Models/Crossy/RoadTiles/kenney-road-row.obj")
    Refs.RailMesh = UAssetManager:GetAssetByPath("Models/Crossy/Trains/crossy-rail-row.obj")
    assert(Refs.GrassMesh and Refs.RoadMesh and Refs.RailMesh, "Map row meshes are missing")
    Refs.DirtWallMesh = UAssetManager:GetAssetByPath("Models/Crossy/RoadTiles/crossy-dirt-wall.obj")
    Refs.TunnelMesh = UAssetManager:GetAssetByPath("Models/Crossy/RoadTiles/crossy-tunnel.obj")
    assert(Refs.DirtWallMesh and Refs.TunnelMesh, "Map wall meshes are missing")
    Refs.WaterMesh = UAssetManager:GetAssetByPath(RiverConfig.WaterMeshPath)
    assert(Refs.WaterMesh, "River water mesh is missing")
    for Slot = 1, Refs.Layout.RowsBehind + Refs.Layout.RowsAhead + 1 do
        local Floor = UStaticMeshComponent.Cast(Actor:FindComponentByName("MapFloor_" .. Slot))
        assert(Floor, "Map pool is incomplete; regenerate the scene")
        local Row = { Slot = Slot, Floor = Floor, Trees = {}, TreeHitboxes = {}, WaterTiles = {}, Active = false, TreeCount = 0 }
        Row.Walls = {}
        for Side = 1, 2 do
            local Name = "MapWall_" .. Slot .. "_" .. Side
            local Wall = UStaticMeshComponent.Cast(Actor:FindComponentByName(Name))
                or UStaticMeshComponent.Cast(Actor:AddComponent(UClass.Find("UStaticMeshComponent"), Name))
            assert(Wall, "Could not create map wall")
            Wall:SetupAttachment(Actor:GetRootComponent())
            Row.Walls[Side] = Wall
        end

        if RiverConfig.WaterMeshMode == "Tile" then local HalfColumns = math.floor(Refs.Layout.HalfWidth / Refs.Layout.CellSize)
            for Column = -HalfColumns, HalfColumns do
                local TileIndex = Column + HalfColumns + 1
                local Name = "MapWater_" .. Slot .. "_" .. TileIndex
                local Water = UStaticMeshComponent.Cast(Actor:FindComponentByName(Name)) or UStaticMeshComponent.Cast(Actor:AddComponent(UClass.Find("UStaticMeshComponent"), Name))
                assert(Water, "Could not create river water tile")
                Water:SetupAttachment(Actor:GetRootComponent())
                Water:SetStaticMesh(Refs.WaterMesh)
                Water:SetCollisionEnabled(ECollisionEnabled.NoCollision)
                Water:SetRelativeRotation(FRotator.new(0, 90, 0))
                Water:SetRelativeScale3D(FVector.new(RiverConfig.WaterTileScale.X, RiverConfig.WaterTileScale.Y, RiverConfig.WaterTileScale.Z))
                Water:SetVisibility(false, true)
                Row.WaterTiles[#Row.WaterTiles + 1] = {Component = Water, Column = Column}
            end
        end

        for Index = 1, TreePoolSize do
            local Tree = GetOrCreateTree(Slot, Index)
            Row.Trees[Index] = Tree
            local Name = "MapTreeHitbox_" .. Slot .. "_" .. Index
            local Hitbox = UBoxComponent.Cast(Actor:FindComponentByName(Name))
                or UBoxComponent.Cast(Actor:AddComponent(UClass.Find("UBoxComponent"), Name))
            assert(Hitbox, "Could not create tree hitbox")
            Hitbox:SetupAttachment(Tree)
            Hitbox:SetBoxExtent(FVector.new(Config.TreeHitboxExtent.X, Config.TreeHitboxExtent.Y, Config.TreeHitboxExtent.Z))
            Hitbox:SetRelativeLocation(FVector.new(0, 0, Config.TreeHitboxExtent.Z))
            Hitbox:SetCollisionObjectType(ECollisionChannel.WorldStatic)
            Hitbox:SetCollisionResponseToAllChannels(ECollisionResponse.Ignore)
            Hitbox:SetCollisionResponseToChannel(ECollisionChannel.Pawn, ECollisionResponse.Block)
            Hitbox:SetCollisionEnabled(ECollisionEnabled.NoCollision)
            Row.TreeHitboxes[Index] = Hitbox
            local Mesh = UStaticMeshComponent.Cast(Actor:FindComponentByName("MapTreeMesh_" .. Slot .. "_" .. Index))
            assert(Mesh, "Missing tree mesh")
            State.TreeMeshesByHitbox[Name] = Mesh
        end
        State.Rows[Slot] = Row
    end
end

function Initialize(Layout)
    Refs.Layout = Layout
    State.HasMapPool = Layout.HasMapPool
    for _, Settings in ipairs({ Config.Day, Config.Night }) do
        local RiverChance = Settings.RiverChance or 0.0
        assert(Settings.RoadChance >= 0 and Settings.RailChance >= 0 and RiverChance >= 0 and Settings.RoadChance + Settings.RailChance + RiverChance <= 1, "Invalid map probabilities")
        assert(Settings.TreesPerRow >= 0 and Settings.TreesPerRow == math.floor(Settings.TreesPerRow), "Tree count must be a non-negative integer")
        assert(#Settings.TreeWeights > 0 and #Settings.TreeWeights <= Settings.TreesPerRow + 1, "Invalid tree counts")
        local Total = 0
        for _, Weight in ipairs(Settings.TreeWeights) do assert(Weight >= 0, "Invalid tree weight") Total = Total + Weight end
        assert(Total > 0, "At least one tree weight must be positive")
    end

    assert(RiverConfig.WaterMeshMode == "Tile" or RiverConfig.WaterMeshMode == "Row", "Unknown water mesh mode")

    for _, Settings in ipairs({ RiverConfig.Day, RiverConfig.Night }) do
        assert(Settings.MinRows >= 1 and Settings.MaxRows >= Settings.MinRows
            and Settings.MinRows == math.floor(Settings.MinRows)
            and Settings.MaxRows == math.floor(Settings.MaxRows), "Invalid river row range")
    end
    CaptureMapPool()
end

function GetPoolSize()
    return #State.Rows, State.HasMapPool and #State.Rows * TreePoolSize or 0
end

-- 내부 행 테이블 대신 매니저 조율에 사용할 값의 복사본 반환
function GetRows()
    local Result = {}
    for Id, Row in ipairs(State.Rows) do
        Result[Id] = { Id = Id, Index = Row.Index, Kind = Row.Kind, Active = Row.Active, TreeCount = Row.TreeCount }
    end
    return Result
end

function ResetGeneration(Seed)
    State.RandomState = Seed
    State.SafeColumn = 0
    State.ReusedRows = 0
    State.LastRow = -Refs.Layout.RowsBehind - 1

    State.RiverRowsRemaining = 0
    State.ForceGrassAfterRiver = false
    State.LastKind = nil
end

function RecycleRow(Id, CountReuse)
    RecycleMapRow(State.Rows[Id])
    if CountReuse then State.ReusedRows = State.ReusedRows + 1 end
end

function GetReusedRows()
    return State.ReusedRows
end

function ReleaseCollisions()
    for Mesh in pairs(State.TreeEffects) do RestoreTreeHit(Mesh) end
    for _, Row in ipairs(State.Rows) do
        for _, Hitbox in ipairs(Row.TreeHitboxes or {}) do
            Hitbox:SetCollisionEnabled(ECollisionEnabled.NoCollision)
        end
    end
end

function CreateRow(Id, Index, Kind)
    GenerateMapRow(State.Rows[Id], Index, Kind)
    return Id, Index, Kind
end

-- 매니저의 기존 도로·레일 연결 순서 유지를 위한 행 단위 반환
function GenerateNextRow(MinRow, MaxRow)
    while State.LastRow < MaxRow do
        State.LastRow = State.LastRow + 1
        local Kind = PickRowType(State.LastRow)
        State.LastKind = Kind

        if State.LastRow >= MinRow then
            for Id, Row in ipairs(State.Rows) do
                if not Row.Active then
                    return CreateRow(Id, State.LastRow, Kind)
                end
            end
            error("No free map row slot")
        end
    end
    return nil
end

function UpdateBounds(HighestRow)
    if not State.HasMapPool then return end
    local CenterX = (HighestRow + (Refs.Layout.RowsAhead - Refs.Layout.RowsBehind) / 2) * Refs.Layout.CellSize
    local Component = USceneComponent.Cast(Actor:FindComponentByName("MapBackdrop"))
    if Component then
        local Location = Component:GetRelativeLocation()
        Location.X, Location.Z = CenterX, 0.01
        Component:SetRelativeLocation(Location)
    end
end
