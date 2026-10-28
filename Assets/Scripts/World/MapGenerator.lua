-- 지형·나무·벽 슬롯만 소유. 차량·기차 상태는 각 시스템에서 별도 보관
local Config = dofile("Assets/Scripts/ConfigManager.lua").Map
State = { Rows = {}, RandomState = 1, LastRow = 0, SafeColumn = 0, ReusedRows = 0 }
Refs = {}

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
    if Index < 3 then return "grass" end
    local Settings = GetSettings()
    local Choice = TerrainRandom()
    if Choice < Settings.RailChance then return "rail" end
    return Choice < Settings.RailChance + Settings.RoadChance and "road" or "grass"
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
    Row.Floor:SetStaticMesh(Kind == "rail" and Refs.RailMesh or (Kind == "road" and Refs.RoadMesh or Refs.GrassMesh))
    Row.Floor:SetRelativeLocation(FVector.new(Index * Refs.Layout.CellSize, 0, 0))
    -- 잔디는 단일 배경 바닥으로 표시하여 행 타일의 경계선과 옆면을 없앤다.
    Row.Floor:SetVisibility(Kind ~= "grass", true)
    -- 바닥 끝(마지막 이동 칸의 바깥쪽)부터 벽 배치. 터널은 행마다 독립 배치
    for Side, Wall in ipairs(Row.Walls) do
        local Sign = Side == 1 and -1 or 1
        Wall:SetStaticMesh(Kind == "grass" and Refs.DirtWallMesh or Refs.TunnelMesh)
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

    for _, Hitbox in ipairs(Row.TreeHitboxes or {}) do Hitbox:SetCollisionEnabled(ECollisionEnabled.NoCollision) end
    if Row.Floor then
        Row.Floor:SetVisibility(false, true)
        for _, Wall in ipairs(Row.Walls) do Wall:SetVisibility(false, true) end
        for _, Tree in ipairs(Row.Trees) do Tree:SetVisibility(false, true) end
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

    for Slot = 1, Refs.Layout.RowsBehind + Refs.Layout.RowsAhead + 1 do
        local Floor = UStaticMeshComponent.Cast(Actor:FindComponentByName("MapFloor_" .. Slot))
        assert(Floor, "Map pool is incomplete; regenerate the scene")
        local Row = { Slot = Slot, Floor = Floor, Trees = {}, TreeHitboxes = {}, Active = false, TreeCount = 0 }
        Row.Walls = {}
        for Side = 1, 2 do
            local Name = "MapWall_" .. Slot .. "_" .. Side
            local Wall = UStaticMeshComponent.Cast(Actor:FindComponentByName(Name))
                or UStaticMeshComponent.Cast(Actor:AddComponent(UClass.Find("UStaticMeshComponent"), Name))
            assert(Wall, "Could not create map wall")
            Wall:SetupAttachment(Actor:GetRootComponent())
            Row.Walls[Side] = Wall
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
        end
        State.Rows[Slot] = Row
    end
end

function Initialize(Layout)
    Refs.Layout = Layout
    State.HasMapPool = Layout.HasMapPool
    for _, Settings in ipairs({ Config.Day, Config.Night }) do
        assert(Settings.RoadChance >= 0 and Settings.RailChance >= 0
            and Settings.RoadChance + Settings.RailChance <= 1, "Invalid map probabilities")
        assert(Settings.TreesPerRow >= 0 and Settings.TreesPerRow == math.floor(Settings.TreesPerRow),
            "Tree count must be a non-negative integer")
        assert(#Settings.TreeWeights > 0 and #Settings.TreeWeights <= Settings.TreesPerRow + 1, "Invalid tree counts")
        local Total = 0
        for _, Weight in ipairs(Settings.TreeWeights) do
            assert(Weight >= 0, "Invalid tree weight")
            Total = Total + Weight
        end
        assert(Total > 0, "At least one tree weight must be positive")
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
    State.RandomState, State.SafeColumn, State.ReusedRows = Seed, 0, 0
    State.LastRow = -Refs.Layout.RowsBehind - 1
end

function RecycleRow(Id, CountReuse)
    RecycleMapRow(State.Rows[Id])
    if CountReuse then State.ReusedRows = State.ReusedRows + 1 end
end

function GetReusedRows()
    return State.ReusedRows
end

function ReleaseCollisions()
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
