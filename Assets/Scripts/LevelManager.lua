-- X축은 진행 방향, Y축은 차량 이동 방향입니다. 위치는 매니저 액터 기준입니다.
local PoolCount = 100
local LaneSpacing = 3.0
local RoadHalfWidth = 34.0
local MinSpawnInterval = 0.25
local MaxSpawnInterval = 2.5
local MinSpeed = 8.0
local MaxSpeed = 20.0

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
-- 배치를 기다리는 비활성 차량 액터입니다.
local AvailableCars = {}
local PoolByActor = {}
local LevelManager = nil
local bWasPlaying = false
local LastRestartCount = 0

local function GetSpawnInterval()
    return MinSpawnInterval + math.random() * (MaxSpawnInterval - MinSpawnInterval)
end

function ReturnCar(CarActor)
    local Entry = PoolByActor[CarActor:GetUUID()]
    if not Entry or Entry.bAvailable then
        return
    end

    Entry.bAvailable = true
    AvailableCars[#AvailableCars + 1] = CarActor
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

    local Entry = { Actor = CarActor, Script = LuaComponent, bAvailable = false }
    Pool[#Pool + 1] = Entry
    PoolByActor[CarActor:GetUUID()] = Entry

    LuaComponent:Invoke("SetCar", Car, LevelManager)
    LuaComponent:Invoke("Deactivate")
end

function SpawnCar(Location, Direction, InSpeed, TravelDistance)
    if #AvailableCars == 0 then
        return nil
    end

    local Index = math.random(1, #AvailableCars)
    local CarActor = AvailableCars[Index]
    AvailableCars[Index] = AvailableCars[#AvailableCars]
    AvailableCars[#AvailableCars] = nil

    local Entry = PoolByActor[CarActor:GetUUID()]
    Entry.bAvailable = false
    Entry.Script:Invoke("Activate", Location, Direction, InSpeed, TravelDistance)
    return Entry
end

local function SpawnLaneCar(Lane, Offset)
    local Sign = Lane.Direction and 1.0 or -1.0
    local Direction = FVector.new(0.0, Sign, 0.0)
    local Location = FVector.new(6.0 + Lane.Index * LaneSpacing, -Sign * RoadHalfWidth, 0.0)
        + Direction * Offset
    local RemainingDistance = RoadHalfWidth * 2.0 - Offset
    local Car = SpawnCar(Location, Direction, Lane.Speed, RemainingDistance)

    if Car then
        Car.Script:Invoke("SetDirection", Lane.Direction)
    end
end

local function SpawnInitialTraffic()
    -- 특정 차선이 풀을 모두 사용하지 않도록 차선별 초기 수량을 제한합니다.
    local MaxCarsPerLane = math.floor(#Pool / #ActiveLane)

    for _, Lane in ipairs(ActiveLane) do
        local Offset = Lane.Speed * GetSpawnInterval() * math.random()

        for _ = 1, MaxCarsPerLane do
            if Offset >= RoadHalfWidth * 2.0 then
                break
            end

            SpawnLaneCar(Lane, Offset)
            Offset = Offset + Lane.Speed * GetSpawnInterval()
        end
    end
end

local function SpawnTraffic(DeltaSeconds)
    for _, Lane in ipairs(ActiveLane) do
        -- NextSpawnTime은 다음 생성까지 남은 게임 시간(초)입니다.
        Lane.NextSpawnTime = Lane.NextSpawnTime - DeltaSeconds
        if Lane.NextSpawnTime <= 0.0 then
            SpawnLaneCar(Lane, 0.0)
            Lane.NextSpawnTime = GetSpawnInterval()
        end
    end
end

function BeginPlay(Self)
    LevelManager = Self
    local GameState = _G.CrossyGameState
    LastRestartCount = GameState and GameState.RestartCount or 0

    for _, Lane in ipairs(ActiveLane) do
        Lane.Speed = MinSpeed + math.random() * (MaxSpeed - MinSpeed)
        Lane.Direction = math.random(0, 1) == 1
        Lane.NextSpawnTime = GetSpawnInterval()
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

    SpawnInitialTraffic()
    bWasPlaying = true
end

function TickComponent(Self, DeltaSeconds)
    local GameState = _G.CrossyGameState

    local bPlaying = GameState and GameState.Phase == "Playing"
    if not bPlaying then
        bWasPlaying = false

        -- 게임 오버 중에도 교통은 유지하고, 준비 및 클리어 상태에서는 멈춥니다.
        if not GameState or GameState.Phase ~= "Dead" then
            return
        end
    end

    local RestartCount = GameState.RestartCount or 0
    if bPlaying and (not bWasPlaying or LastRestartCount ~= RestartCount) then
        for _, Entry in ipairs(Pool) do
            Entry.Script:Invoke("Deactivate")
        end

        for _, Lane in ipairs(ActiveLane) do
            Lane.NextSpawnTime = GetSpawnInterval()
        end

        -- 재시작 시에도 빈 도로에서 기다리지 않도록 다시 배치합니다.
        SpawnInitialTraffic()
        bWasPlaying = true
        LastRestartCount = RestartCount
    end

    SpawnTraffic(DeltaSeconds)
end

function EndPlay(Self)
    -- 종료 중인 차량은 더 이상 반환받지 않습니다.
    PoolByActor = {}
    AvailableCars = {}

    for _, Entry in ipairs(Pool) do
        Entry.Script:Invoke("SetCar", nil, nil)
        Entry.Actor:Destroy()
    end

    Pool = {}
    LevelManager = nil
    bWasPlaying = false
end
