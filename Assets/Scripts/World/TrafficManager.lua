-- 차량 풀과 도로별 생성 상태만 소유. 지형 행 테이블의 직접 수정 제외
local Config = dofile("Assets/Scripts/ConfigManager.lua").Traffic
State = { CarPool = {}, AvailableCars = {}, PoolByActor = {}, Lanes = {}, Enabled = false, MaxCarLength = 0.0, CarLimit = 34.0 }
Refs = {}

local function GetDifficulty()
    return Refs.Layout.NightMode and Config.Night or Config.Day
end

function BeginPlay(Self)
    Self:SetComponentTickEnabled(false)
end

function Initialize(Self, Level, Player, PlayerScript, Layout)
    Refs.Self, Refs.Level, Refs.Player, Refs.PlayerScript, Refs.Layout = Self, Level, Player, PlayerScript, Layout
    State.CarLimit = Layout.RoadHalfWidth
    for _, Settings in ipairs({ Config.Day, Config.Night }) do
        assert(Settings.MinSpeed > 0 and Settings.MaxSpeed >= Settings.MinSpeed, "Invalid road speed range")
        assert(Settings.MinSpawnInterval > 0 and Settings.MaxSpawnInterval >= Settings.MinSpawnInterval,
            "Invalid car spawn interval")
    end
    assert(Config.PositiveYChance >= 0 and Config.PositiveYChance <= 1, "Invalid direction probability")
end

-- 기차 대기 시간과 동일한 기존 교통 난수 스트림 사용
local function TrafficRandom()
    return Refs.Level:Invoke("TrafficRandom")
end

local function SpawnInterval(Speed)
    local Settings = GetDifficulty()
    local Minimum = Settings.MinSpawnInterval
    -- 차량 길이·속도를 반영해 같은 도로에서 생성 직후 겹침 방지
    if Speed then
        if Speed <= 0 then return math.huge end
        Minimum = math.max(Minimum, (State.MaxCarLength + Config.MinCarGap) / Speed)
    end
    local Maximum = math.max(Minimum, Settings.MaxSpawnInterval)
    return Minimum + TrafficRandom() * (Maximum - Minimum)
end

function ReturnCar(CarActor)
    local Entry = State.PoolByActor[CarActor:GetUUID()]
    if not Entry or Entry.bAvailable then return end

    if Entry.Row then
        local Cars = Entry.Row.Cars
        for Index, Other in ipairs(Cars) do
            if Other == Entry then
                Cars[Index] = Cars[#Cars]
                Cars[#Cars] = nil
                break
            end
        end
        Entry.Row = nil
    end

    Entry.bAvailable = true
    State.AvailableCars[#State.AvailableCars + 1] = Entry
end

local function CreateCarPool()
    local WeightSum = 0
    for _, Car in ipairs(Config.Cars) do
        assert(Car.Weight >= 0, "Car weights must be non-negative")
        WeightSum = WeightSum + Car.Weight
    end
    assert(WeightSum > 0, "At least one car weight must be positive")

    -- 가중치에 따른 정수 대수 배분 후 소수 부분이 큰 차종부터 나머지 배정
    local Counts, Remainders, Assigned = {}, {}, 0
    for Index, Car in ipairs(Config.Cars) do
        local Exact = Config.PoolCount * Car.Weight / WeightSum
        Counts[Index], Remainders[Index] = math.floor(Exact), Exact - math.floor(Exact)
        Assigned = Assigned + Counts[Index]
    end

    for _ = Assigned + 1, Config.PoolCount do
        local Best = 1
        for Index = 2, #Config.Cars do if Remainders[Index] > Remainders[Best] then Best = Index end end
        Counts[Best], Remainders[Best] = Counts[Best] + 1, -1
    end

    for Index, Car in ipairs(Config.Cars) do
        for _ = 1, Counts[Index] do
            local Pose = FTransform.new()
            Pose.Location = FVector.new(0, 0, -1000)
            local CarActor = Actor:GetWorld():SpawnActor(UClass.Find("AStaticMeshActor"), "Car " .. (#State.CarPool + 1), Pose)
            local Mesh = UStaticMeshComponent.Cast(CarActor:FindComponentByName("UStaticMeshComponent"))
            local Asset = UAssetManager:GetAssetByPath(Car.Path)
            assert(Mesh and Asset, "Missing car mesh: " .. Car.Path)
            Mesh:SetStaticMesh(Asset)
            local Bounds, Position = Mesh:CalcBounds(), CarActor:GetActorLocation()
            local Scale = Config.CarWidth / (Bounds.Max.Y - Bounds.Min.Y)
            State.MaxCarLength = math.max(State.MaxCarLength, (Bounds.Max.X - Bounds.Min.X) * Scale)
            Mesh:SetRelativeScale3D(FVector.new(Scale, Scale, Scale))
            Mesh:SetRelativeLocation(FVector.new(
                -(Bounds.Min.X + Bounds.Max.X - 2 * Position.X) * Scale / 2,
                -(Bounds.Min.Y + Bounds.Max.Y - 2 * Position.Y) * Scale / 2,
                -(Bounds.Min.Z - Position.Z) * Scale))
            State.CarLimit = math.max(State.CarLimit, Refs.Layout.RoadHalfWidth + (Bounds.Max.X - Bounds.Min.X) * Scale / 2 + Refs.Layout.CellSize)
            do -- 낮에도 조명을 생성해 모드 전환 시 재사용한다.
                -- 차체 루트에 연결해 진행 방향 전환과 풀의 표시 상태를 함께 따른다.
                for Index, Side in ipairs({ -1, 1 }) do
                    local Light = USpotLightComponent.Cast(CarActor:AddComponent(UClass.Find("USpotLightComponent"), "NightHeadlight_" .. Index))
                    assert(Light, "Could not create car headlight")
                    Light:SetupAttachment(CarActor:GetRootComponent())
                    Light:SetRelativeLocation(FVector.new(-(Bounds.Max.X - Bounds.Min.X) * Scale / 2,
                        Side * Config.CarWidth * Config.Headlight.SideOffsetRatio, Config.Headlight.Height))
                    Light:SetRelativeRotation(FRotator.new(Config.Headlight.Pitch, Config.Headlight.Yaw, 0))
                    Refs.Level:Invoke("RegisterLight", "Car", Light)
                    Light:SetCastShadows(Config.Headlight.CastShadows)
                end
            end

            local Script = ULuaComponent.Cast(CarActor:AddComponent(ULuaComponent.StaticClass(), "CarScript"))
            Script:SetScriptPath("Scripts/Car.lua")
            Script:InitializeLua()
            assert(Script:LoadLua(), "Car.lua load failed")
            local Entry = { Actor = CarActor, Script = Script, bAvailable = false }
            State.CarPool[#State.CarPool + 1], State.PoolByActor[CarActor:GetUUID()] = Entry, Entry
            Script:Invoke("SetCar", Car, Refs.Self, Refs.Level, Refs.Player, Refs.PlayerScript)
            Script:Invoke("Deactivate")
        end
    end
end

function SpawnCar(Location, Direction, Speed, TravelDistance)
    if #State.AvailableCars == 0 then return nil end

    local Index = math.floor(TrafficRandom() * #State.AvailableCars) + 1
    local Entry = State.AvailableCars[Index]
    State.AvailableCars[Index] = State.AvailableCars[#State.AvailableCars]
    State.AvailableCars[#State.AvailableCars] = nil

    Entry.bAvailable = false
    Entry.Script:Invoke("Activate", Location, Direction, Speed, TravelDistance)
    return Entry
end

local function SpawnRowCar(Row, Offset)
    local Sign = Row.Direction and 1 or -1
    local SpawnY = Refs.Layout.Origin.Y + Sign * (-State.CarLimit + Offset)
    -- 프레임 지연과 차량/매니저 Tick 순서에 관계없이 실제 거리도 확인
    for _, Other in ipairs(Row.Cars) do
        if math.abs(Other.Actor:GetActorLocation().Y - SpawnY) < State.MaxCarLength + Config.MinCarGap then
            return
        end
    end
    local Entry = SpawnCar(FVector.new(Refs.Layout.Origin.X + Row.Index * Refs.Layout.CellSize,
        SpawnY, Refs.Layout.Origin.Z),
        FVector.new(0, Sign, 0), Row.Speed, State.CarLimit * 2 - Offset)
    if Entry then
        Entry.Row = Row
        Row.Cars[#Row.Cars + 1] = Entry
        Entry.Script:Invoke("SetDirection", Row.Direction)
    end
end

local function PopulateRoad(Row)
    if not State.Enabled or Row.Kind ~= "road" then return end
    local RoadCount = 0
    for _, Other in pairs(State.Lanes) do if Other.Active and Other.Kind == "road" then RoadCount = RoadCount + 1 end end
    local Count = math.min(Config.MaxInitialCarsPerRow, math.floor(Config.PoolCount / math.max(1, RoadCount)))
    local Offset = Row.Speed * SpawnInterval(Row.Speed) * TrafficRandom()
    for _ = 1, Count do
        if Offset >= State.CarLimit * 2 then break end
        SpawnRowCar(Row, Offset)
        Offset = Offset + Row.Speed * SpawnInterval(Row.Speed)
    end
    Row.NextSpawnTime = SpawnInterval(Row.Speed)
end

function Update(DeltaSeconds)
    if not State.Enabled then return end
    for Id = 1, Refs.Layout.RowCount do
        local Row = State.Lanes[Id]
        if Row then
            Row.NextSpawnTime = Row.NextSpawnTime - DeltaSeconds
            if Row.NextSpawnTime <= 0 then
                SpawnRowCar(Row, 0)
                Row.NextSpawnTime = SpawnInterval(Row.Speed)
            end
        end
    end
end

function DestroyCars()
    State.PoolByActor, State.AvailableCars = {}, {}
    for _, Entry in ipairs(State.CarPool) do
        Entry.Script:Invoke("SetCar", nil, nil, nil, nil, nil)
        Entry.Actor:Destroy()
    end
    State.CarPool = {}
end

-- 모든 종류의 행에서 기존 순서대로 난수 소비. 실제 차량 상태는 도로에만 보관
function PrepareRow(Id, Index, Kind)
    local NextSpawnTime = SpawnInterval()
    local Direction, Speed
    if Kind == "road" or Kind == "rail" then
        Direction = TrafficRandom() < Config.PositiveYChance
        local Settings = GetDifficulty()
        Speed = Settings.MinSpeed + TrafficRandom() * (Settings.MaxSpeed - Settings.MinSpeed)
    end
    if Kind == "road" then
        State.Lanes[Id] = { Index = Index, Kind = Kind, Active = true, Cars = {}, Direction = Direction, Speed = Speed, NextSpawnTime = NextSpawnTime }
    end
    return Direction, Speed
end

function PopulateRow(Id)
    PopulateRoad(State.Lanes[Id])
end

function RemoveRow(Id)
    local Row = State.Lanes[Id]
    if not Row then return end
    for Index = #Row.Cars, 1, -1 do Row.Cars[Index].Script:Invoke("Deactivate") end
    State.Lanes[Id] = nil
end

function DeactivateCars()
    for _, Entry in ipairs(State.CarPool) do Entry.Script:Invoke("Deactivate") end
end

function EnableTraffic()
    if State.Enabled then return end
    CreateCarPool()
    State.Enabled = true
    for Id = 1, Refs.Layout.RowCount do
        local Row = State.Lanes[Id]
        if Row then PopulateRoad(Row) end
    end
end

function IsEnabled()
    return State.Enabled
end

function GetCarCount()
    return #State.CarPool - #State.AvailableCars, #State.CarPool
end

function GetLaneInfo(Id)
    local Row = State.Lanes[Id]
    return { Index = Row.Index, Direction = Row.Direction, Speed = Row.Speed, TrainActive = false }
end
