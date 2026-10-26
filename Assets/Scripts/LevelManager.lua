-- 좌표: +X 전진 / Y 차량 이동 / Z 높이. 한 줄 중심에서 다음 중심까지 3 World Units.
-- 1. 맵 설정: 다음 Play부터 적용됩니다. R은 같은 설정으로 새 맵을 뽑습니다.
local LaneSpacing = 3.0
local RowsBehind = 10
local DeathRowsBehind = 6
local RowsAhead = 30
local MapPlayerHalfWidth = 24.0
local MapBoundaryWidth = 0.3
local MapRoadChance = 0.60
local MapRailChance = 0.05 -- 나머지 35%는 잔디. 시작 0~2행은 안전한 잔디입니다.
local MapTreeCountWeights = { 5, 15, 20, 30, 20, 10 } -- 나무 0~5개의 상대 가중치
local TreesPerRow = 5
local MapTreeHitboxExtent = FVector.new(0.45, 0.45, 1.0) -- 나무 밑동의 반너비/반높이
local InitialSeed = 20261026

-- 2. 도로/차량 설정: 같은 도로의 차량은 방향과 속도를 공유합니다.
local PoolCount = 100
local RoadHalfWidth = 34.0
local CarWidth = 2.0
local MinCarGap = 0.8 -- 차량 사이에 유지할 최소 여유 거리 (World Units)
local MaxCarLength = 0.0 -- 풀의 메시 크기와 배율로 계산한 최대 차량 길이
local MaxInitialCarsPerRow = 4
local MinSpawnInterval = 0.25
local MaxSpawnInterval = 2.5
local MapPositiveYChance = 0.50
local MapMinLaneSpeed = 8.0
local MapMaxLaneSpeed = 20.0
local MinSpeed = 8.0 -- 기존 고정 맵 씬도 원격 밸런스를 적용합니다.
local MaxSpeed = 20.0
local CarList = {
    { Path = "Models/Crossy/Cars/taxi.obj", Weight = 1.0 },
    { Path = "Models/Crossy/Cars/sedan.obj", Weight = 1.0 },
    { Path = "Models/Crossy/Cars/truck.obj", Weight = 1.0 }
}

-- 3. 기차 설정: 레일마다 3종 중 하나를 선택하고, 통과 후 다시 출발합니다.
local MapTrainSpeed = 50.0
local MapTrainWaitMin = 3.0
local MapTrainWaitMax = 6.0
local MapTrainTypes = {
    { Name = "bullet", Path = "Models/Crossy/Trains/crossy-train-long.obj" },
    { Name = "city", Path = "Models/Crossy/Trains/crossy-train-city.obj" },
    { Name = "freight", Path = "Models/Crossy/Trains/crossy-train-freight.obj" }
}

-- 기존 고정 맵 씬의 차량 배치는 유지합니다. 새 메인 씬은 생성된 도로 행을 사용합니다.
local StaticRoadRows = { 1, 2, 6, 7, 8, 11, 12, 16, 17, 18, 21, 22, 26, 27, 31, 32, 33 }
local LevelManager, Player, PlayerScript, PlayerMesh = nil, nil, nil, nil
local Origin, StartPosition, StartMeshLocation, StartMeshRotation, StartMeshScale = nil, nil, nil, nil, nil
local Rows, CarPool, AvailableCars, PoolByActor = {}, {}, {}, {}
local HasMapPool, TrafficEnabled = false, false
local HighestRow, LastRow, SafeColumn = 0, 0, 0
local Seed, TerrainRandomState, TrafficRandomState, TrainRandomState = InitialSeed, InitialSeed, 1, 1
local GrassMesh, RoadMesh, RailMesh = nil, nil, nil
local LevelInfo = nil
local ReusedRows, CarLimit = 0, RoadHalfWidth

-- 랜덤 상태를 분리해 기차 종류나 차량 배치가 지형/나무 배치를 바꾸지 않습니다.
local function TerrainRandom()
    TerrainRandomState = TerrainRandomState * 48271 % 2147483647
    return TerrainRandomState / 2147483647
end
local function TerrainInteger(Min, Max)
    return Min + math.floor(TerrainRandom() * (Max - Min + 1))
end
local function TrafficRandom()
    TrafficRandomState = TrafficRandomState * 48271 % 2147483647
    return TrafficRandomState / 2147483647
end
local function SpawnInterval(Speed)
    local Minimum = MinSpawnInterval
    if Speed then
        if Speed <= 0 then return math.huge end
        Minimum = math.max(Minimum, (MaxCarLength + MinCarGap) / Speed)
    end
    local Maximum = math.max(Minimum, MaxSpawnInterval)
    return Minimum + TrafficRandom() * (Maximum - Minimum)
end
local function PickRowType(Index)
    if Index < 3 then return "grass" end
    local Choice = TerrainRandom()
    if Choice < MapRailChance then return "rail" end
    return Choice < MapRailChance + MapRoadChance and "road" or "grass"
end
local function PickTreeCount()
    local Total = 0
    for _, Weight in ipairs(MapTreeCountWeights) do Total = Total + Weight end
    local Choice = TerrainRandom() * Total
    for Index, Weight in ipairs(MapTreeCountWeights) do
        Choice = Choice - Weight
        if Choice < 0 then return Index - 1 end
    end
    return #MapTreeCountWeights - 1
end

-- 차량 풀: 생성은 시작할 때 한 번, 이후에는 배치/반환만 합니다.
function ReturnCar(CarActor)
    local Entry = PoolByActor[CarActor:GetUUID()]
    if not Entry or Entry.bAvailable then return end
    if Entry.Row then
        local Cars = Entry.Row.Cars
        for Index, Other in ipairs(Cars) do
            if Other == Entry then Cars[Index] = Cars[#Cars]; Cars[#Cars] = nil; break end
        end
        Entry.Row = nil
    end
    Entry.bAvailable = true
    AvailableCars[#AvailableCars + 1] = Entry
end

local function CreateCarPool()
    local WeightSum = 0
    for _, Car in ipairs(CarList) do
        assert(Car.Weight >= 0, "Car weights must be non-negative")
        WeightSum = WeightSum + Car.Weight
    end
    assert(WeightSum > 0, "At least one car weight must be positive")
    local Counts, Remainders, Assigned = {}, {}, 0
    for Index, Car in ipairs(CarList) do
        local Exact = PoolCount * Car.Weight / WeightSum
        Counts[Index], Remainders[Index] = math.floor(Exact), Exact - math.floor(Exact)
        Assigned = Assigned + Counts[Index]
    end
    for _ = Assigned + 1, PoolCount do
        local Best = 1
        for Index = 2, #CarList do if Remainders[Index] > Remainders[Best] then Best = Index end end
        Counts[Best], Remainders[Best] = Counts[Best] + 1, -1
    end
    for Index, Car in ipairs(CarList) do
        for _ = 1, Counts[Index] do
            local Pose = FTransform.new()
            Pose.Location = FVector.new(0, 0, -1000)
            local CarActor = Actor:GetWorld():SpawnActor(UClass.Find("AStaticMeshActor"), "Car " .. (#CarPool + 1), Pose)
            local Mesh = UStaticMeshComponent.Cast(CarActor:FindComponentByName("UStaticMeshComponent"))
            local Asset = UAssetManager:GetAssetByPath(Car.Path)
            assert(Mesh and Asset, "Missing car mesh: " .. Car.Path)
            Mesh:SetStaticMesh(Asset)
            local Bounds, Position = Mesh:CalcBounds(), CarActor:GetActorLocation()
            local Scale = CarWidth / (Bounds.Max.Y - Bounds.Min.Y)
            MaxCarLength = math.max(MaxCarLength, (Bounds.Max.X - Bounds.Min.X) * Scale)
            Mesh:SetRelativeScale3D(FVector.new(Scale, Scale, Scale))
            Mesh:SetRelativeLocation(FVector.new(
                -(Bounds.Min.X + Bounds.Max.X - 2 * Position.X) * Scale / 2,
                -(Bounds.Min.Y + Bounds.Max.Y - 2 * Position.Y) * Scale / 2,
                -(Bounds.Min.Z - Position.Z) * Scale))
            CarLimit = math.max(CarLimit, RoadHalfWidth + (Bounds.Max.X - Bounds.Min.X) * Scale / 2 + LaneSpacing)
            local Script = ULuaComponent.Cast(CarActor:AddComponent(ULuaComponent.StaticClass(), "CarScript"))
            Script:SetScriptPath("Scripts/Car.lua")
            Script:InitializeLua()
            assert(Script:LoadLua(), "Car.lua load failed")
            local Entry = { Actor = CarActor, Script = Script, bAvailable = false }
            CarPool[#CarPool + 1], PoolByActor[CarActor:GetUUID()] = Entry, Entry
            Script:Invoke("SetCar", Car, LevelManager)
            Script:Invoke("Deactivate")
        end
    end
end

function SpawnCar(Location, Direction, Speed, TravelDistance)
    if #AvailableCars == 0 then return nil end
    local Index = math.floor(TrafficRandom() * #AvailableCars) + 1
    local Entry = AvailableCars[Index]
    AvailableCars[Index] = AvailableCars[#AvailableCars]
    AvailableCars[#AvailableCars] = nil
    Entry.bAvailable = false
    Entry.Script:Invoke("Activate", Location, Direction, Speed, TravelDistance)
    return Entry
end

local function SpawnRowCar(Row, Offset)
    local Sign = Row.Direction and 1 or -1
    local SpawnY = Origin.Y + Sign * (-CarLimit + Offset)
    -- 프레임 지연과 차량/매니저 Tick 순서에 관계없이 실제 거리도 확인합니다.
    for _, Other in ipairs(Row.Cars) do
        if math.abs(Other.Actor:GetActorLocation().Y - SpawnY) < MaxCarLength + MinCarGap then
            return
        end
    end
    local Entry = SpawnCar(FVector.new(Origin.X + Row.Index * LaneSpacing,
        SpawnY, Origin.Z),
        FVector.new(0, Sign, 0), Row.Speed, CarLimit * 2 - Offset)
    if Entry then
        Entry.Row = Row
        Row.Cars[#Row.Cars + 1] = Entry
        Entry.Script:Invoke("SetDirection", Row.Direction)
    end
end

local function PopulateRoad(Row)
    if not TrafficEnabled or Row.Kind ~= "road" then return end
    local RoadCount = 0
    for _, Other in ipairs(Rows) do if Other.Active and Other.Kind == "road" then RoadCount = RoadCount + 1 end end
    local Count = math.min(MaxInitialCarsPerRow, math.floor(PoolCount / math.max(1, RoadCount)))
    local Offset = Row.Speed * SpawnInterval(Row.Speed) * TrafficRandom()
    for _ = 1, Count do
        if Offset >= CarLimit * 2 then break end
        SpawnRowCar(Row, Offset)
        Offset = Offset + Row.Speed * SpawnInterval(Row.Speed)
    end
    Row.NextSpawnTime = SpawnInterval(Row.Speed)
end

local function UpdateRoadTraffic(DeltaSeconds)
    if not TrafficEnabled then return end
    for _, Row in ipairs(Rows) do
        if Row.Active and Row.Kind == "road" then
            Row.NextSpawnTime = Row.NextSpawnTime - DeltaSeconds
            if Row.NextSpawnTime <= 0 then
                SpawnRowCar(Row, 0)
                Row.NextSpawnTime = SpawnInterval(Row.Speed)
            end
        end
    end
end

-- 맵 회수: 해당 행의 차량 반환 -> 기차 숨김 -> 나무/바닥 숨김 -> 슬롯 해제.
local function RecycleMapRow(Row)
    for Index = #Row.Cars, 1, -1 do Row.Cars[Index].Script:Invoke("Deactivate") end
    Row.Active, Row.TrainActive = false, false
    for _, Hitbox in ipairs(Row.TreeHitboxes or {}) do Hitbox:SetCollisionEnabled(ECollisionEnabled.NoCollision) end
    if Row.Floor then
        Row.Floor:SetVisibility(false, true)
        Row.Train:SetVisibility(false, true)
        for _, Tree in ipairs(Row.Trees) do Tree:SetVisibility(false, true) end
    end
    Row.TrainType, Row.Direction, Row.Speed = nil, nil, nil
end

-- 기차 배치: 종류/길이/방향/속도 결정. 기차 컴포넌트 자체는 재사용합니다.
local function SpawnTrain(Row)
    TrainRandomState = TrainRandomState * 48271 % 2147483647
    Row.TrainType = MapTrainTypes[math.floor(TrainRandomState / 2147483647 * #MapTrainTypes) + 1]
    Row.Train:SetStaticMesh(Row.TrainType.Mesh)
    Row.Train:SetRelativeRotation(FRotator.new(0, 0, 0))
    local Bounds = Row.Train:CalcBounds()
    Row.TrainHalfLength = (Bounds.Max.Y - Bounds.Min.Y) / 2
    Row.Speed, Row.TrainActive = MapTrainSpeed, false
    Row.TrainWait = 1 + TrafficRandom() * 3
    local Sign = Row.Direction and 1 or -1
    Row.Train:SetRelativeRotation(FRotator.new(0, Row.Direction and 0 or 180, 0))
    Row.Train:SetRelativeLocation(FVector.new(Row.Index * LaneSpacing,
        -Sign * (RoadHalfWidth + Row.TrainHalfLength + LaneSpacing), 0))
end

-- 맵 생성: 종류 결정 -> 바닥 교체 -> 도로 차량/레일 기차/잔디 나무 배치.
local function GenerateMapRow(Row, Index, Kind)
    Row.Index, Row.Kind, Row.Active, Row.TreeCount = Index, Kind, true, 0
    Row.Cars, Row.TrainType, Row.TrainActive = {}, nil, false
    Row.NextSpawnTime = SpawnInterval()
    if Kind == "road" or Kind == "rail" then
        Row.Direction = TrafficRandom() < MapPositiveYChance
        local Min = Row.Floor and MapMinLaneSpeed or MinSpeed
        local Max = Row.Floor and MapMaxLaneSpeed or MaxSpeed
        Row.Speed = Min + TrafficRandom() * (Max - Min)
    end
    if not Row.Floor then PopulateRoad(Row); return end
    Row.Floor:SetStaticMesh(Kind == "rail" and RailMesh or (Kind == "road" and RoadMesh or GrassMesh))
    Row.Floor:SetRelativeLocation(FVector.new(Index * LaneSpacing, 0, 0))
    Row.Floor:SetVisibility(true, true)
    Row.Train:SetVisibility(false, true)
    for _, Hitbox in ipairs(Row.TreeHitboxes) do Hitbox:SetCollisionEnabled(ECollisionEnabled.NoCollision) end
    for _, Tree in ipairs(Row.Trees) do Tree:SetVisibility(false, true) end
    if Kind == "rail" then SpawnTrain(Row); return end
    if Kind == "road" then PopulateRoad(Row); return end
    if Index < 3 then return end
    local HalfColumns = math.floor(MapPlayerHalfWidth / LaneSpacing)
    local RouteLimit = math.max(0, HalfColumns - 2)
    local Next = math.max(-RouteLimit, math.min(RouteLimit, SafeColumn + TerrainInteger(-1, 1)))
    local Columns = {}
    for Column = -HalfColumns, HalfColumns do
        if Column < math.min(SafeColumn, Next) or Column > math.max(SafeColumn, Next) then Columns[#Columns + 1] = Column end
    end
    SafeColumn = Next
    for Index = #Columns, 2, -1 do
        local Other = TerrainInteger(1, Index)
        Columns[Index], Columns[Other] = Columns[Other], Columns[Index]
    end
    Row.TreeCount = math.min(PickTreeCount(), #Columns)
    for Index = 1, Row.TreeCount do
        local Tree = Row.Trees[Index]
        Tree:SetRelativeLocation(FVector.new(Row.Index * LaneSpacing, Columns[Index] * LaneSpacing, 0))
        Tree:SetRelativeRotation(FRotator.new(0, TerrainInteger(0, 3) * 90, 0))
        local Scale = 0.9 + TerrainRandom() * 0.1
        Tree:SetRelativeScale3D(FVector.new(Scale, Scale, Scale))
        Tree:SetVisibility(true, true)
        Row.TreeHitboxes[Index]:SetCollisionEnabled(ECollisionEnabled.QueryOnly)
    end
end

local function RecycleMapRows(MinRow)
    for _, Row in ipairs(Rows) do
        if Row.Active and Row.Index < MinRow then RecycleMapRow(Row); ReusedRows = ReusedRows + 1 end
    end
end

local function GenerateMapRows(MinRow, MaxRow)
    while LastRow < MaxRow do
        LastRow = LastRow + 1
        local Kind = PickRowType(LastRow)
        if LastRow >= MinRow then
            local Slot = nil
            for _, Row in ipairs(Rows) do if not Row.Active then Slot = Row; break end end
            assert(Slot, "No free map row slot")
            GenerateMapRow(Slot, LastRow, Kind)
        end
    end
end

local function StopPlayerMovement()
    if PlayerScript then PlayerScript:Invoke("StopMovement") end
end
local function RequestGameOver(Reason)
    local State = _G.CrossyGameState
    if State.Phase ~= "Playing" then return end
    if State.GameOver then State.GameOver()
    else State.Phase = "Dead"; StopPlayerMovement() end
    print("[Level] " .. Reason)
end

local function UpdateTrains(DeltaSeconds)
    local ActiveCount = 0
    for _, Row in ipairs(Rows) do
        if Row.Active and Row.Kind == "rail" then
            local Step = DeltaSeconds
            if not Row.TrainActive then
                Row.TrainWait = Row.TrainWait - DeltaSeconds
                if Row.TrainWait <= 0 then
                    Step, Row.TrainActive = -Row.TrainWait, true
                    Row.Train:SetVisibility(true, true)
                end
            end
            if Row.TrainActive then
                local Sign = Row.Direction and 1 or -1
                local Limit = RoadHalfWidth + Row.TrainHalfLength + LaneSpacing
                local Position = Row.Train:GetRelativeLocation()
                local NextY = math.max(-Limit, math.min(Limit, Position.Y + Sign * Row.Speed * Step))
                local Bounds, DeltaY = Row.Train:CalcBounds(), NextY - Position.Y
                local HasBounds, PlayerBounds = Player:TryGetActorBounds()
                local bHit = _G.CrossyGameState.Phase == "Playing" and HasBounds
                    and Bounds.Min.X <= PlayerBounds.Max.X and Bounds.Max.X >= PlayerBounds.Min.X
                    and Bounds.Min.Z <= PlayerBounds.Max.Z and Bounds.Max.Z >= PlayerBounds.Min.Z
                    and math.min(Bounds.Min.Y, Bounds.Min.Y + DeltaY) <= PlayerBounds.Max.Y
                    and math.max(Bounds.Max.Y, Bounds.Max.Y + DeltaY) >= PlayerBounds.Min.Y
                Position.Y = NextY
                Row.Train:SetRelativeLocation(Position)
                if bHit then RequestGameOver("기차와 충돌했습니다.") end
                if Sign * NextY >= Limit then
                    Row.TrainActive = false
                    Row.Train:SetVisibility(false, true)
                    Position.Y = -Sign * Limit
                    Row.Train:SetRelativeLocation(Position)
                    Row.TrainWait = MapTrainWaitMin + TrafficRandom() * (MapTrainWaitMax - MapTrainWaitMin)
                else ActiveCount = ActiveCount + 1 end
            end
        end
    end
    LevelInfo.ActiveTrains = ActiveCount
end

local function PublishLevelState()
    LevelInfo.HighestRow, LevelInfo.ReusedRows = HighestRow, ReusedRows
    LevelInfo.MinRow, LevelInfo.MaxRow = HighestRow - RowsBehind, HighestRow + RowsAhead
    LevelInfo.ActiveRows, LevelInfo.RoadRows, LevelInfo.GrassRows, LevelInfo.RailRows, LevelInfo.ActiveTrees = 0, 0, 0, 0, 0
    LevelInfo.RoadLanes, LevelInfo.RailLanes = {}, {}
    for _, Row in ipairs(Rows) do
        if Row.Active then
            LevelInfo.ActiveRows = LevelInfo.ActiveRows + 1
            LevelInfo.ActiveTrees = LevelInfo.ActiveTrees + Row.TreeCount
            if Row.Kind == "grass" then LevelInfo.GrassRows = LevelInfo.GrassRows + 1
            else
                local IsRail = Row.Kind == "rail"
                local Lanes = IsRail and LevelInfo.RailLanes or LevelInfo.RoadLanes
                Lanes[Row.Index] = { Index = Row.Index, X = Origin.X + Row.Index * LaneSpacing,
                    Direction = Row.Direction, Speed = Row.Speed, TrainType = Row.TrainType and Row.TrainType.Name or nil }
                if IsRail then LevelInfo.RailRows = LevelInfo.RailRows + 1 else LevelInfo.RoadRows = LevelInfo.RoadRows + 1 end
            end
        end
    end
    if not HasMapPool then return end
    local CenterX = (HighestRow + (RowsAhead - RowsBehind) / 2) * LaneSpacing
    for _, Name in ipairs({ "MapBackdrop", "MapBoundaryLeft", "MapBoundaryRight" }) do
        local Component = USceneComponent.Cast(Actor:FindComponentByName(Name))
        if Component then
            local Location = Component:GetRelativeLocation()
            Location.X, Location.Z = CenterX, 0.01
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
end

-- 전체 레일 속도를 바꾸거나 RowIndex를 지정해 한 레일의 속도만 바꿉니다.
function SetTrainSpeed(Speed, RowIndex)
    assert(Speed > 0, "Train speed must be positive")
    if RowIndex == nil then MapTrainSpeed = Speed end
    for _, Row in ipairs(Rows) do
        if Row.Active and Row.Kind == "rail" and (RowIndex == nil or Row.Index == RowIndex) then Row.Speed = Speed end
    end
    PublishLevelState()
end

local function ResetLevel(NewSeed)
    for _, Entry in ipairs(CarPool) do Entry.Script:Invoke("Deactivate") end
    for _, Row in ipairs(Rows) do RecycleMapRow(Row) end
    if NewSeed then Seed = Seed + 1 end
    TerrainRandomState, TrafficRandomState, TrainRandomState = Seed, (Seed + 104729) % 2147483647, (Seed + 65537) % 2147483647
    HighestRow, LastRow, SafeColumn, ReusedRows = 0, -RowsBehind - 1, 0, 0
    LevelInfo.ActiveTrains, LevelInfo.Seed = 0, Seed
    if HasMapPool then GenerateMapRows(-RowsBehind, RowsAhead)
    else for Index, Row in ipairs(Rows) do GenerateMapRow(Row, StaticRoadRows[Index], "road") end end
    PublishLevelState()
end

local function EnableTraffic()
    if TrafficEnabled then return end
    CreateCarPool()
    TrafficEnabled = true
    for _, Row in ipairs(Rows) do if Row.Active then PopulateRoad(Row) end end
end

local function CaptureMapPool()
    if not HasMapPool then
        for _ = 1, #StaticRoadRows do Rows[#Rows + 1] = { Cars = {}, Active = false } end
        return
    end
    GrassMesh = UAssetManager:GetAssetByPath("Models/Crossy/RoadTiles/kenney-grass-row.obj")
    RoadMesh = UAssetManager:GetAssetByPath("Models/Crossy/RoadTiles/kenney-road-row.obj")
    RailMesh = UAssetManager:GetAssetByPath("Models/Crossy/Trains/crossy-rail-row.obj")
    assert(GrassMesh and RoadMesh and RailMesh, "Map row meshes are missing")
    for _, Type in ipairs(MapTrainTypes) do
        Type.Mesh = UAssetManager:GetAssetByPath(Type.Path)
        assert(Type.Mesh, "Missing train: " .. Type.Path)
    end
    for Slot = 1, RowsBehind + RowsAhead + 1 do
        local Floor = UStaticMeshComponent.Cast(Actor:FindComponentByName("MapFloor_" .. Slot))
        local Train = UStaticMeshComponent.Cast(Actor:FindComponentByName("MapTrain_" .. Slot))
        assert(Floor and Train, "Map pool is incomplete; regenerate the scene")
        local Row = { Floor = Floor, Train = Train, Trees = {}, TreeHitboxes = {}, Cars = {}, Active = false }
        for Index = 1, TreesPerRow do
            local Tree = USceneComponent.Cast(Actor:FindComponentByName("MapTree_" .. Slot .. "_" .. Index))
            assert(Tree, "Missing tree slot")
            Row.Trees[Index] = Tree
            local Name = "MapTreeHitbox_" .. Slot .. "_" .. Index
            local Hitbox = UBoxComponent.Cast(Actor:FindComponentByName(Name))
                or UBoxComponent.Cast(Actor:AddComponent(UClass.Find("UBoxComponent"), Name))
            assert(Hitbox, "Could not create tree hitbox")
            Hitbox:SetupAttachment(Tree)
            Hitbox:SetBoxExtent(MapTreeHitboxExtent)
            Hitbox:SetRelativeLocation(FVector.new(0, 0, MapTreeHitboxExtent.Z))
            Hitbox:SetCollisionObjectType(ECollisionChannel.WorldStatic)
            Hitbox:SetCollisionResponseToAllChannels(ECollisionResponse.Ignore)
            Hitbox:SetCollisionResponseToChannel(ECollisionChannel.Pawn, ECollisionResponse.Block)
            Hitbox:SetCollisionEnabled(ECollisionEnabled.NoCollision)
            Row.TreeHitboxes[Index] = Hitbox
        end
        Rows[Slot] = Row
    end
end

local function PrintLevelState()
    print("[Level] rows", LevelInfo.MinRow, "..", LevelInfo.MaxRow, "active", LevelInfo.ActiveRows,
        "road/grass/rail", LevelInfo.RoadRows, LevelInfo.GrassRows, LevelInfo.RailRows,
        "cars", #CarPool - #AvailableCars, "/", #CarPool, "trains", LevelInfo.ActiveTrains, "reused", ReusedRows)
    for Index = LevelInfo.MinRow, LevelInfo.MaxRow do
        local Lane = LevelInfo.RoadLanes[Index] or LevelInfo.RailLanes[Index]
        if Lane then print("[Lane]", Index, Lane.Direction and "+Y" or "-Y", "speed", Lane.Speed, Lane.TrainType or "car") end
    end
end

-- 시작: 설정 확인 -> 풀 참조 -> 초기 맵 생성. GameManager가 차량과 게임 상태를 활성화합니다.
function BeginPlay(Self)
    assert(DeathRowsBehind > 0 and DeathRowsBehind <= RowsBehind, "Death boundary must be inside the retained map")
    assert(MapRoadChance >= 0 and MapRailChance >= 0 and MapRoadChance + MapRailChance <= 1, "Invalid map probabilities")
    assert(MapTrainSpeed > 0 and MapTrainWaitMin >= 0 and MapTrainWaitMax >= MapTrainWaitMin, "Invalid train settings")
    assert(MapMinLaneSpeed > 0 and MapMaxLaneSpeed >= MapMinLaneSpeed, "Invalid road speed range")
    assert(MapPositiveYChance >= 0 and MapPositiveYChance <= 1, "Invalid direction probability")
    assert(MapPlayerHalfWidth >= LaneSpacing and MapPlayerHalfWidth <= 33 and MapBoundaryWidth > 0, "Invalid map width")
    local Total = 0
    assert(#MapTreeCountWeights > 0 and #MapTreeCountWeights <= TreesPerRow + 1, "Invalid tree counts")
    for _, Weight in ipairs(MapTreeCountWeights) do assert(Weight >= 0, "Invalid tree weight"); Total = Total + Weight end
    assert(Total > 0, "At least one tree weight must be positive")
    LevelManager, Player = Self, Actor:GetWorld():GetPlayerPawn()
    assert(Player, "Select a Player Pawn")
    StartPosition = Player:GetActorLocation()
    HasMapPool = Actor:FindComponentByName("MapFloor_1") ~= nil
    if HasMapPool then Actor:SetActorEnableCollision(true) end
    Origin = HasMapPool and Actor:GetActorLocation() or FVector.new(6, 0, 0)
    for _, Component in ipairs(Player:GetComponents()) do
        local Script, Mesh = ULuaComponent.Cast(Component), UStaticMeshComponent.Cast(Component)
        if Script and Script:GetScriptPath() == "Scripts/Player.lua" then PlayerScript = Script end
        if Mesh and not PlayerMesh then
            PlayerMesh = Mesh
            StartMeshLocation, StartMeshRotation, StartMeshScale = Mesh:GetRelativeLocation(), Mesh:GetRelativeRotation(), Mesh:GetRelativeScale3D()
        end
    end
    _G.CrossyGameState = _G.CrossyGameState or { Phase = "Playing", Score = 0, MaxRow = 0 }
    LevelInfo = { IsEndless = HasMapPool, CellSize = LaneSpacing, StartX = StartPosition.X,
        MinY = Origin.Y - (HasMapPool and MapPlayerHalfWidth or 33), MaxY = Origin.Y + (HasMapPool and MapPlayerHalfWidth or 33),
        PoolRows = #Rows, PoolTrees = 0, ActiveTrains = 0, Reset = ResetLevel, EnableTraffic = EnableTraffic,
        SetTrainSpeed = SetTrainSpeed }
    _G.CrossyLevel = LevelInfo
    -- 이전 maptest용 Player.lua와의 호환 별칭. 게임 상태는 이 테이블에 넣지 않습니다.
    if HasMapPool then _G.CrossyMapTest = LevelInfo end
    CaptureMapPool()
    LevelInfo.PoolRows, LevelInfo.PoolTrees = #Rows, HasMapPool and #Rows * TreesPerRow or 0
    ResetLevel(false)
    if _G.CrossyGameState.EnableTraffic then EnableTraffic() end
end

-- 매 프레임: 전진 확인 -> 오래된 행 회수 -> 앞쪽 행 생성 -> 차량/기차 갱신.
function TickComponent(Self, DeltaSeconds)
    local State = _G.CrossyGameState
    if not State then return end
    if State.EnableTraffic and not TrafficEnabled then EnableTraffic() end
    -- GameManager가 없는 maptest만 자체 R키와 점수를 처리합니다.
    if not State.ManagedByGameManager and Input.IsKeyPressed(EKeyCode.R) then
        StopPlayerMovement()
        Player:GetRootComponent():SetWorldLocation(StartPosition, false)
        if PlayerMesh then
            PlayerMesh:SetRelativeLocation(StartMeshLocation)
            PlayerMesh:SetRelativeRotation(StartMeshRotation)
            PlayerMesh:SetRelativeScale3D(StartMeshScale)
        end
        State.Score, State.MaxRow, State.Phase = 0, 0, "Playing"
        ResetLevel(true)
    end
    if State.Phase == "Playing" and HasMapPool then
        local Position = Player:GetActorLocation()
        local DeathX = Origin.X + (HighestRow - DeathRowsBehind) * LaneSpacing
        if Position.X < DeathX - 0.0001 then
            Player:GetRootComponent():SetWorldLocation(FVector.new(DeathX, Position.Y, Position.Z), false)
            RequestGameOver("뒤쪽 사망 경계를 벗어났습니다.")
        else
            local Current = math.floor((Position.X - Origin.X) / LaneSpacing + 0.0001)
            if Current > HighestRow then
                HighestRow = Current
                RecycleMapRows(HighestRow - RowsBehind)
                GenerateMapRows(HighestRow - RowsBehind, HighestRow + RowsAhead)
                PublishLevelState()
            end
            if not State.ManagedByGameManager then State.MaxRow, State.Score = HighestRow, HighestRow end
        end
    end
    -- __Crossy의 동작 유지: 사망 후에도 교통은 흐르고, 준비/클리어에서는 멈춥니다.
    if State.Phase == "Playing" or State.Phase == "Dead" then
        UpdateRoadTraffic(DeltaSeconds)
        if HasMapPool then UpdateTrains(DeltaSeconds) end
    end
    LevelInfo.Cars = #CarPool - #AvailableCars
    if Input.IsKeyPressed(EKeyCode.P) then PrintLevelState() end
end

function EndPlay(Self)
    for _, Row in ipairs(Rows) do
        for _, Hitbox in ipairs(Row.TreeHitboxes or {}) do Hitbox:SetCollisionEnabled(ECollisionEnabled.NoCollision) end
    end
    PoolByActor, AvailableCars = {}, {}
    for _, Entry in ipairs(CarPool) do
        Entry.Script:Invoke("SetCar", nil, nil)
        Entry.Actor:Destroy()
    end
    CarPool, Rows, LevelManager = {}, {}, nil
    if _G.CrossyLevel == LevelInfo then _G.CrossyLevel, _G.CrossyMapTest = nil, nil end
    if _G.CrossyGameState and not _G.CrossyGameState.ManagedByGameManager then _G.CrossyGameState = nil end
end
