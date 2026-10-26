-- X축은 진행 방향, Y축은 차량 이동 방향입니다. 위치는 매니저 액터 기준입니다.
local PoolCount = 100
local LaneSpacing = 3.0
local RoadHalfWidth = 34.0
local SpawnInterval = 5.0
local MinSpeed = 3.0
local MaxSpeed = 8.0

-- maptest 설정: 수정 후 다음 Play부터 적용됩니다. R은 현재 설정으로 맵을 다시 뽑습니다.
local RowsBehind = 10 -- 뒤쪽 맵 유지 범위
local DeathRowsBehind = 6 -- 최고 도달 행 기준 뒤쪽 사망 경계
local RowsAhead = 30
local MapPlayerHalfWidth = 24.0 -- 좌우 이동 한계: -24 ~ +24
local MapBoundaryWidth = 0.3 -- 양쪽 흰색 띠의 폭
local MapRoadChance = 0.60 -- 시작 안전 구간 제외: 도로 60%, 레일 5%, 나머지 잔디
local MapRailChance = 0.05
local MapPositiveYChance = 0.50 -- 도로가 +Y 방향일 확률. 나머지는 -Y
local MapMinLaneSpeed = 3.0 -- 도로별 차량 속도 범위 (World Units / 초)
local MapMaxLaneSpeed = 8.0
local MapTrainSpeed = 28.0 -- 레일 한 줄의 기차 속도 (World Units / 초)
local MapTrainWaitMin = 3.0 -- 한 편성이 완전히 지나간 뒤 다음 출발까지의 대기 시간
local MapTrainWaitMax = 6.0
-- 0개, 1개, 2개, 3개, 4개, 5개의 상대 가중치. 합계는 100이 아니어도 됩니다.
local MapTreeCountWeights = { 5, 10, 20, 30, 25, 10 }

local ActiveLane = {
    { Index = 1, Speed = 1.0, Direction = false },
    { Index = 2, Speed = 1.0, Direction = false },

    { Index = 6, Speed = 1.0, Direction = false },
    { Index = 7, Speed = 1.0, Direction = false },
    { Index = 8, Speed = 1.0, Direction = false },

    { Index = 11, Speed = 1.0, Direction = false },
    { Index = 12, Speed = 1.0, Direction = false },

    { Index = 16, Speed = 1.0, Direction = false },
    { Index = 17, Speed = 1.0, Direction = false },
    { Index = 18, Speed = 1.0, Direction = false },

    { Index = 21, Speed = 1.0, Direction = false },
    { Index = 22, Speed = 1.0, Direction = false },

    { Index = 26, Speed = 1.0, Direction = false },
    { Index = 27, Speed = 1.0, Direction = false },

    { Index = 31, Speed = 1.0, Direction = false },
    { Index = 32, Speed = 1.0, Direction = false },
    { Index = 33, Speed = 1.0, Direction = false }
}

local CarList = {
    { Path = "Models/Crossy/Cars/taxi.obj", Size = 1.0, Weight = 1.0 },
    { Path = "Models/Crossy/Cars/sedan.obj", Size = 1.0, Weight = 1.0 },
    { Path = "Models/Crossy/Cars/truck.obj", Size = 1.0, Weight = 1.0 }
}

local Pool = {}
local SpawnElapsed = 0.0
local bWasPlaying = false

-- maptest.scene은 일반 차량 없이 맵/기차 컴포넌트를 풀링합니다.
local bMapTestMode = false
local MapPoolCount = RowsBehind + RowsAhead + 1
local TreesPerRow = 5
local MapRows = {}
local MapPlayer, MapPlayerScript, MapPlayerMesh = nil, nil, nil
local MapStartPosition, MapStartMeshLocation, MapStartMeshRotation, MapOrigin = nil, nil, nil, nil
local MapHighestRow, MapLastRow, MapSafeColumn = 0, -11, 0
local MapSeed, MapRandomState = 20261026, 20261026
local MapTrafficRandomState = 1
local MapGrassMesh, MapRoadMesh, MapRailMesh = nil, nil, nil
local MapTrainHalfLength = 0

local function MapRandom()
    MapRandomState = (MapRandomState * 48271) % 2147483647
    return MapRandomState / 2147483647
end

local function MapInteger(Min, Max)
    return Min + math.floor(MapRandom() * (Max - Min + 1))
end

-- 교통 설정을 추가해도 도로/잔디와 나무의 랜덤 배치는 바뀌지 않습니다.
local function MapTrafficRandom()
    MapTrafficRandomState = (MapTrafficRandomState * 48271) % 2147483647
    return MapTrafficRandomState / 2147483647
end

local function MapRowType(Index)
    if Index < 3 then return "grass" end
    local Choice = MapRandom()
    if Choice < MapRailChance then return "rail" end
    return Choice < MapRailChance + MapRoadChance and "road" or "grass"
end

local function MapTreeCount()
    local Total = 0
    for _, Weight in ipairs(MapTreeCountWeights) do Total = Total + Weight end
    local Choice = MapRandom() * Total
    for Index, Weight in ipairs(MapTreeCountWeights) do
        Choice = Choice - Weight
        if Choice < 0 then return Index - 1 end
    end
    return #MapTreeCountWeights - 1
end

local function ReleaseMapRow(Row)
    Row.Active = false
    Row.Direction, Row.Speed = nil, nil
    Row.TrainActive, Row.TrainWait = false, 0
    Row.Train:SetVisibility(false, true)
    Row.Floor:SetVisibility(false, true)
    for _, Tree in ipairs(Row.Trees) do Tree:SetVisibility(false, true) end
end

local function PlaceMapRow(Row, Index, Kind)
    Row.Index, Row.Kind, Row.Active, Row.TreeCount = Index, Kind, true, 0
    Row.Direction, Row.Speed = nil, nil
    if Kind == "road" or Kind == "rail" then
        -- 같은 도로의 차량들이 공유할 값. 회수 후 새 도로로 배치할 때 다시 뽑습니다.
        Row.Direction = MapTrafficRandom() < MapPositiveYChance -- true: +Y, false: -Y
        if Kind == "rail" then
            Row.Speed = MapTrainSpeed
        else
            Row.Speed = MapMinLaneSpeed + MapTrafficRandom() * (MapMaxLaneSpeed - MapMinLaneSpeed)
        end
    end
    Row.TrainActive, Row.TrainWait = false, 0
    Row.Train:SetVisibility(false, true)
    if Kind == "rail" then
        local Sign = Row.Direction and 1 or -1
        Row.TrainWait = 1.0 + MapTrafficRandom() * 3.0
        Row.Train:SetRelativeRotation(FRotator.new(0, Row.Direction and 0 or 180, 0))
        Row.Train:SetRelativeLocation(FVector.new(Index * LaneSpacing,
            -Sign * (RoadHalfWidth + MapTrainHalfLength + LaneSpacing), 0))
    end
    Row.Floor:SetStaticMesh(Kind == "rail" and MapRailMesh or (Kind == "road" and MapRoadMesh or MapGrassMesh))
    Row.Floor:SetRelativeLocation(FVector.new(Index * LaneSpacing, 0.0, 0.0))
    Row.Floor:SetVisibility(true, true)
    for _, Tree in ipairs(Row.Trees) do Tree:SetVisibility(false, true) end
    if Kind ~= "grass" or Index < 3 then return end

    local HalfColumns = math.floor(MapPlayerHalfWidth / LaneSpacing)
    local RouteLimit = math.max(0, HalfColumns - 2)
    local NextColumn = math.max(-RouteLimit, math.min(RouteLimit, MapSafeColumn + MapInteger(-1, 1)))
    local Columns = {}
    for Column = -HalfColumns, HalfColumns do
        if Column < math.min(MapSafeColumn, NextColumn) or Column > math.max(MapSafeColumn, NextColumn) then
            Columns[#Columns + 1] = Column
        end
    end
    MapSafeColumn = NextColumn
    for Index = #Columns, 2, -1 do
        local Other = MapInteger(1, Index)
        Columns[Index], Columns[Other] = Columns[Other], Columns[Index]
    end
    Row.TreeCount = math.min(MapTreeCount(), #Columns)
    for TreeIndex = 1, Row.TreeCount do
        local Tree = Row.Trees[TreeIndex]
        Tree:SetRelativeLocation(FVector.new(Index * LaneSpacing, Columns[TreeIndex] * LaneSpacing, 0.0))
        Tree:SetRelativeRotation(FRotator.new(0.0, MapInteger(0, 3) * 90.0, 0.0))
        local Scale = 0.9 + MapRandom() * 0.1
        Tree:SetRelativeScale3D(FVector.new(Scale, Scale, Scale))
        Tree:SetVisibility(true, true)
    end
end

local function UpdateMapStats()
    local Stats = _G.CrossyMapTest
    Stats.HighestRow = MapHighestRow
    Stats.MinRow, Stats.MaxRow = MapHighestRow - RowsBehind, MapHighestRow + RowsAhead
    Stats.MinY, Stats.MaxY = MapOrigin.Y - MapPlayerHalfWidth, MapOrigin.Y + MapPlayerHalfWidth
    Stats.ActiveRows, Stats.RoadRows, Stats.GrassRows, Stats.ActiveTrees = 0, 0, 0, 0
    Stats.RailRows, Stats.ActiveTrains = 0, 0
    Stats.RoadLanes = {} -- 풀 슬롯 번호가 아닌 실제 도로 행 번호로 조회합니다.
    Stats.RailLanes = {}
    for _, Row in ipairs(MapRows) do
        if Row.Active then
            Stats.ActiveRows = Stats.ActiveRows + 1
            Stats.ActiveTrees = Stats.ActiveTrees + Row.TreeCount
            if Row.Kind == "road" or Row.Kind == "rail" then
                local Lanes = Row.Kind == "rail" and Stats.RailLanes or Stats.RoadLanes
                if Row.Kind == "rail" then
                    Stats.RailRows = Stats.RailRows + 1
                    if Row.TrainActive then Stats.ActiveTrains = Stats.ActiveTrains + 1 end
                else Stats.RoadRows = Stats.RoadRows + 1 end
                Lanes[Row.Index] = {
                    Index = Row.Index, X = MapOrigin.X + Row.Index * LaneSpacing,
                    Direction = Row.Direction, Speed = Row.Speed
                }
            else Stats.GrassRows = Stats.GrassRows + 1 end
        end
    end
    local CenterX = (MapHighestRow + (RowsAhead - RowsBehind) / 2) * LaneSpacing
    for _, Name in ipairs({ "MapBackdrop", "MapBoundaryLeft", "MapBoundaryRight" }) do
        local Component = USceneComponent.Cast(Actor:FindComponentByName(Name))
        if Component then
            local Location = Component:GetRelativeLocation()
            Location.X = CenterX
	    Location.Z = 0.01
            if Name ~= "MapBackdrop" then
                local Sign = Name == "MapBoundaryLeft" and -1 or 1
                Location.Y = Sign * (MapPlayerHalfWidth + LaneSpacing / 2 + MapBoundaryWidth / 2)
                local Scale = Component:GetRelativeScale3D()
                Scale.Y = MapBoundaryWidth / LaneSpacing
                Component:SetRelativeScale3D(Scale)
            end
            Component:SetRelativeLocation(Location)
        end
    end
    local State = _G.CrossyGameState
    State.MaxRow, State.Score = MapHighestRow, MapHighestRow
end

local function PrintMapStats()
    local Stats = _G.CrossyMapTest
    print("[MapTest] rows", Stats.MinRow, "..", Stats.MaxRow,
        "active", Stats.ActiveRows, "pool", MapPoolCount,
        "road", Stats.RoadRows, "grass", Stats.GrassRows, "rail", Stats.RailRows,
        "trains", Stats.ActiveTrains,
        "trees", Stats.ActiveTrees, "reused", Stats.ReusedRows, "cars", 0)
end

local function PrintMapLanes()
    local Stats = _G.CrossyMapTest
    for Index = Stats.MinRow, Stats.MaxRow do
        local Lane = Stats.RoadLanes[Index] or Stats.RailLanes[Index]
        if Lane then
            print("[MapTest]", Stats.RailLanes[Index] and "rail" or "road", Index,
                "direction", Lane.Direction and "+Y" or "-Y",
                "speed", math.floor(Lane.Speed * 100 + 0.5) / 100)
        end
    end
end

local function RestoreMapPlayer(Position, bReload)
    if bReload and MapPlayerScript then
        assert(MapPlayerScript:LoadLua(), "MapTest: Player.lua reload failed")
    end
    MapPlayer:GetRootComponent():SetWorldLocation(Position, false)
    if MapPlayerMesh then
        MapPlayerMesh:SetRelativeLocation(MapStartMeshLocation)
        MapPlayerMesh:SetRelativeRotation(MapStartMeshRotation)
    end
end

local function StopMapTest(Position, Reason)
    _G.CrossyGameState.Phase = "Dead"
    RestoreMapPlayer(Position, true)
    print("[MapTest] " .. Reason .. " R키로 재시작하세요.")
end

local function TickMapTrains(DeltaSeconds, PlayerPosition)
    local ActiveCount = 0
    for _, Row in ipairs(MapRows) do
        if Row.Active and Row.Kind == "rail" then
            local Step = DeltaSeconds
            if not Row.TrainActive then
                Row.TrainWait = Row.TrainWait - DeltaSeconds
                if Row.TrainWait <= 0 then
                    Step = -Row.TrainWait
                    Row.TrainActive = true
                    Row.Train:SetVisibility(true, true)
                end
            end
            if Row.TrainActive then
                local Sign = Row.Direction and 1 or -1
                local Limit = RoadHalfWidth + MapTrainHalfLength + LaneSpacing
                local Location = Row.Train:GetRelativeLocation()
                local NextY = math.max(-Limit, math.min(Limit, Location.Y + Sign * Row.Speed * Step))
                local DeltaY = NextY - Location.Y
                local Bounds = Row.Train:CalcBounds()
                local HasPlayerBounds, PlayerBounds = MapPlayer:TryGetActorBounds()
                -- 빠른 기차가 한 프레임에 플레이어를 통과해도 이동 구간 전체를 검사합니다.
                local bHit = HasPlayerBounds
                    and Bounds.Min.X <= PlayerBounds.Max.X and Bounds.Max.X >= PlayerBounds.Min.X
                    and Bounds.Min.Z <= PlayerBounds.Max.Z and Bounds.Max.Z >= PlayerBounds.Min.Z
                    and math.min(Bounds.Min.Y, Bounds.Min.Y + DeltaY) <= PlayerBounds.Max.Y
                    and math.max(Bounds.Max.Y, Bounds.Max.Y + DeltaY) >= PlayerBounds.Min.Y
                Location.Y = NextY
                Row.Train:SetRelativeLocation(Location)
                if bHit then StopMapTest(PlayerPosition, "기차와 충돌했습니다."); return end
                if Sign * NextY >= Limit then
                    Row.TrainActive = false
                    Row.Train:SetVisibility(false, true)
                    Location.Y = -Sign * Limit
                    Row.Train:SetRelativeLocation(Location)
                    Row.TrainWait = MapTrainWaitMin + MapTrafficRandom() * (MapTrainWaitMax - MapTrainWaitMin)
                else ActiveCount = ActiveCount + 1 end
            end
        end
    end
    _G.CrossyMapTest.ActiveTrains = ActiveCount
end

local function ResetMapTest(bRestart)
    local State = _G.CrossyGameState
    State.Phase = "Ready"
    if bRestart then MapSeed = MapSeed + 1 end
    MapRandomState = MapSeed
    MapTrafficRandomState = (MapSeed + 104729) % 2147483647
    if MapTrafficRandomState == 0 then MapTrafficRandomState = 1 end
    MapHighestRow, MapLastRow, MapSafeColumn = 0, -RowsBehind - 1, 0
    _G.CrossyMapTest = { Seed = MapSeed, PoolRows = MapPoolCount, PoolTrees = MapPoolCount * TreesPerRow,
        ReusedRows = 0, Cars = 0 }
    for Slot, Row in ipairs(MapRows) do
        ReleaseMapRow(Row)
        local Index = Slot - RowsBehind - 1
        PlaceMapRow(Row, Index, MapRowType(Index))
        MapLastRow = Index
    end
    RestoreMapPlayer(MapStartPosition, bRestart)
    State.GameOverRequested, State.ClearRequested = false, false
    State.Phase = "Playing"
    UpdateMapStats()
    PrintMapStats()
end

local function BeginMapTest()
    assert(DeathRowsBehind > 0 and DeathRowsBehind <= RowsBehind, "DeathRowsBehind must be positive and no greater than RowsBehind")
    assert(MapRoadChance >= 0 and MapRailChance >= 0 and MapRoadChance + MapRailChance <= 1, "Road and rail probabilities must sum to at most 1")
    assert(MapTrainSpeed > 0 and MapTrainWaitMin >= 0 and MapTrainWaitMax >= MapTrainWaitMin, "Invalid train speed or wait range")
    assert(MapPositiveYChance >= 0 and MapPositiveYChance <= 1, "MapPositiveYChance must be between 0 and 1")
    assert(MapMinLaneSpeed >= 0 and MapMaxLaneSpeed >= MapMinLaneSpeed, "Map lane speed range must satisfy 0 <= min <= max")
    assert(MapPlayerHalfWidth >= LaneSpacing and MapPlayerHalfWidth <= 33, "MapPlayerHalfWidth must be between 3 and 33")
    assert(MapBoundaryWidth > 0, "MapBoundaryWidth must be positive")
    assert(#MapTreeCountWeights > 0 and #MapTreeCountWeights <= TreesPerRow + 1, "Tree weights support counts 0 through 5")
    local TotalWeight = 0
    for _, Weight in ipairs(MapTreeCountWeights) do
        assert(Weight >= 0, "Tree count weights must be non-negative")
        TotalWeight = TotalWeight + Weight
    end
    assert(TotalWeight > 0, "At least one tree count weight must be positive")
    MapPlayer = Actor:GetWorld():GetPlayerPawn()
    assert(MapPlayer, "MapTest: select a Player Pawn")
    MapStartPosition, MapOrigin = MapPlayer:GetActorLocation(), Actor:GetActorLocation()
    for _, Component in ipairs(MapPlayer:GetComponents()) do
        local Script = ULuaComponent.Cast(Component)
        if Script and Script:GetScriptPath() == "Scripts/Player.lua" then MapPlayerScript = Script end
        local Mesh = UStaticMeshComponent.Cast(Component)
        if Mesh and not MapPlayerMesh then
            MapPlayerMesh = Mesh
            MapStartMeshLocation, MapStartMeshRotation = Mesh:GetRelativeLocation(), Mesh:GetRelativeRotation()
        end
    end
    MapGrassMesh = UAssetManager:GetAssetByPath("Models/Crossy/RoadTiles/kenney-grass-row.obj")
    MapRoadMesh = UAssetManager:GetAssetByPath("Models/Crossy/RoadTiles/kenney-road-row.obj")
    MapRailMesh = UAssetManager:GetAssetByPath("Models/Crossy/Trains/crossy-rail-row.obj")
    assert(MapGrassMesh and MapRoadMesh and MapRailMesh, "MapTest: row meshes are missing")
    MapRows = {}
    for Slot = 1, MapPoolCount do
        local Floor = UStaticMeshComponent.Cast(Actor:FindComponentByName("MapFloor_" .. Slot))
        assert(Floor, "MapTest: missing floor slot " .. Slot)
        local Train = UStaticMeshComponent.Cast(Actor:FindComponentByName("MapTrain_" .. Slot))
        assert(Train, "MapTest: regenerate maptest.scene to add train slots")
        if Slot == 1 then
            local Bounds = Train:CalcBounds()
            MapTrainHalfLength = (Bounds.Max.Y - Bounds.Min.Y) / 2
        end
        local Row = { Floor = Floor, Train = Train, Trees = {}, Active = false }
        for TreeIndex = 1, TreesPerRow do
            local Tree = USceneComponent.Cast(Actor:FindComponentByName("MapTree_" .. Slot .. "_" .. TreeIndex))
            assert(Tree, "MapTest: missing tree slot")
            Row.Trees[TreeIndex] = Tree
        end
        MapRows[Slot] = Row
    end
    _G.CrossyGameState = { Phase = "Ready", Score = 0, MaxRow = 0 }
    MapSeed = 20261026
    ResetMapTest(false)
    print("[MapTest] WASD move; P pool stats and lane settings; R reset. Rail 5%; trains enabled; no cars.")
end

local function TickMapTest(DeltaSeconds)
    if Input.IsKeyPressed(EKeyCode.R) then ResetMapTest(true); return end
    if Input.IsKeyPressed(EKeyCode.P) then PrintMapStats(); PrintMapLanes() end
    local State = _G.CrossyGameState
    if State.Phase ~= "Playing" then return end
    local Position = MapPlayer:GetActorLocation()
    local DeathX = MapOrigin.X + (MapHighestRow - DeathRowsBehind) * LaneSpacing
    -- 맵 회수 범위와 별개인 사망 경계에서 중지하며, 현재 점프 상태도 초기화합니다.
    if Position.X < DeathX - 0.0001 then
        StopMapTest(FVector.new(DeathX, Position.Y, MapStartPosition.Z),
            "뒤쪽 " .. DeathRowsBehind .. "칸 경계를 벗어났습니다.")
        return
    end
    TickMapTrains(DeltaSeconds, Position)
    if State.Phase ~= "Playing" then return end
    local CurrentRow = math.floor((Position.X - MapOrigin.X) / LaneSpacing + 0.0001)
    if CurrentRow <= MapHighestRow then return end
    MapHighestRow = CurrentRow
    local MinRow, MaxRow = MapHighestRow - RowsBehind, MapHighestRow + RowsAhead
    for _, Row in ipairs(MapRows) do
        if Row.Index < MinRow then
            ReleaseMapRow(Row)
            _G.CrossyMapTest.ReusedRows = _G.CrossyMapTest.ReusedRows + 1
        end
    end
    -- 디버그 이동으로 여러 칸을 건너뛰어도 최종 41줄만 배치합니다.
    while MapLastRow < MaxRow do
        MapLastRow = MapLastRow + 1
        local Kind = MapRowType(MapLastRow)
        if MapLastRow >= MinRow then
            local Available = nil
            for _, Row in ipairs(MapRows) do
                if not Row.Active then Available = Row; break end
            end
            assert(Available, "MapTest: no free row slot")
            PlaceMapRow(Available, MapLastRow, Kind)
        end
    end
    UpdateMapStats()
end

local function CreateCar(Index, Car)
    local World = Actor:GetWorld()
    local Class = UClass.Find("AStaticMeshActor")
    local Name = "Car " .. Index
    local Transform = FTransform.new()
    Transform.Location = FVector.new(0.0, 0.0, -1000.0)
    local CarActor = World:SpawnActor(Class, Name, Transform)
    
    -- Car Mesh 붙이기
    local StaticMeshComponent = UStaticMeshComponent.Cast(
        CarActor:FindComponentByName("UStaticMeshComponent")
    )
    local CarMesh = UAssetManager:GetAssetByPath(Car.Path)
    StaticMeshComponent:SetStaticMesh(CarMesh)

    -- Lua 스크립트 붙이기
    local Component = CarActor:AddComponent(ULuaComponent.StaticClass(), "CarScript")
    local LuaComponent = ULuaComponent.Cast(Component)
    LuaComponent:SetScriptPath("Scripts/Car.lua")

    -- Lua 스크립트 로드
    LuaComponent:InitializeLua()
    if not LuaComponent:LoadLua() then
        CarActor:Destroy()
        return nil
    end

    LuaComponent:Invoke("SetCar", Car)
    LuaComponent:Invoke("Deactivate")
    Pool[#Pool + 1] = { Actor = CarActor, Script = LuaComponent }
end

function SpawnCar(Location, Direction, InSpeed, TravelDistance)
    for _, Entry in ipairs(Pool) do
        if not Entry.Script:Invoke("IsActive") then
            Entry.Script:Invoke("Activate", Location, Direction, InSpeed, TravelDistance)
            return Entry
        end
    end

    -- 풀이 가득 차면 추가 생성 없이 이번 배치를 건너뜁니다.
    return nil
end

local function SpawnTraffic()
    local Origin = FVector.new(6.0, 0.0, 0.0)

    for _, Lane in ipairs(ActiveLane) do
        local Sign = Lane.Direction and 1.0 or -1.0
        local Location = Origin + FVector.new(Lane.Index * LaneSpacing, -Sign * RoadHalfWidth, 0.0)
        local Car = SpawnCar(Location, FVector.new(0.0, Sign, 0.0), Lane.Speed, RoadHalfWidth * 2.0)

        if (Car) then
            Car.Script:Invoke("SetDirection", Lane.Direction)
        end
    end
end

function BeginPlay(Self)
    bMapTestMode = Actor:FindComponentByName("MapFloor_1") ~= nil
    if bMapTestMode then BeginMapTest(); return end

    for _, Lane in ipairs(ActiveLane) do
        Lane.Speed = MinSpeed + math.random() * (MaxSpeed - MinSpeed)
        Lane.Direction = math.random(0, 1) == 1
    end

    local WeightSum = 0.0
    for _, Car in ipairs(CarList) do
        assert(Car.Weight >= 0.0, "Car Weight must be non-negative")
        WeightSum = WeightSum + Car.Weight
    end
    assert(WeightSum > 0.0, "Car WeightSum must be positive")

    local Counts = {}
    local Remainders = {}
    local AssignedCount = 0

    for Index, Car in ipairs(CarList) do
        local ExactCount = PoolCount * Car.Weight / WeightSum
        Counts[Index] = math.floor(ExactCount)
        Remainders[Index] = ExactCount - Counts[Index]
        AssignedCount = AssignedCount + Counts[Index]
    end

    -- 정수로 나누어떨어지지 않는 수량은 소수 부분이 큰 종류부터 배분합니다.
    for _ = AssignedCount + 1, PoolCount do
        local BestIndex = 1
        for Index = 2, #CarList do
            if Remainders[Index] > Remainders[BestIndex] then
                BestIndex = Index
            end
        end

        Counts[BestIndex] = Counts[BestIndex] + 1
        Remainders[BestIndex] = -1.0
    end

    local PoolIndex = 0
    for Index, Car in ipairs(CarList) do
        for _ = 1, Counts[Index] do
            PoolIndex = PoolIndex + 1
            CreateCar(PoolIndex, Car)
        end
    end
end

function TickComponent(Self, DeltaSeconds)
    if bMapTestMode then TickMapTest(DeltaSeconds); return end

    local GameState = _G.CrossyGameState
    if not GameState or GameState.Phase ~= "Playing" then
        if bWasPlaying then
            for _, Entry in ipairs(Pool) do
                Entry.Script:Invoke("Deactivate")
            end

            SpawnElapsed = 0.0
            bWasPlaying = false
        end
        return
    end

    bWasPlaying = true
    SpawnElapsed = SpawnElapsed - DeltaSeconds
    if SpawnElapsed <= 0.0 then
        SpawnElapsed = SpawnInterval
        SpawnTraffic()
    end
end

function EndPlay(Self)
    if bMapTestMode then
        MapRows = {}
        MapPlayer, MapPlayerScript, MapPlayerMesh = nil, nil, nil
        _G.CrossyMapTest, _G.CrossyGameState = nil, nil
        return
    end

    for _, Entry in ipairs(Pool) do
        Entry.Actor:Destroy()
    end

    Pool = {}
    SpawnElapsed = 0.0
end
