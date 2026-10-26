-- X축은 진행 방향, Y축은 차량 이동 방향입니다. 위치는 매니저 액터 기준입니다.
local PoolCount = 100
local LaneSpacing = 3.0
local RoadHalfWidth = 34.0
local MinSpawnInterval = 3.0
local MaxSpawnInterval = 5.0
local MinSpeed = 3.0
local MaxSpeed = 8.0

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
local DebugLogElapsed = 0.0

local function GetSpawnInterval()
    return MinSpawnInterval + math.random() * (MaxSpawnInterval - MinSpawnInterval)
end

function ReturnCar(CarActor)
    local Entry = PoolByActor[CarActor:GetUUID()]
    if not Entry then
        print("[LevelManager] ReturnCar: 풀에 없는 액터", CarActor:GetName())
        return
    end

    if Entry.bAvailable then
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
        print("[LevelManager] SpawnCar: 배치 가능한 차량 없음, Pool:", #Pool)
        return nil
    end

    local Index = math.random(1, #AvailableCars)
    local CarActor = AvailableCars[Index]
    AvailableCars[Index] = AvailableCars[#AvailableCars]
    AvailableCars[#AvailableCars] = nil

    local Entry = PoolByActor[CarActor:GetUUID()]
    Entry.bAvailable = false
    Entry.Script:Invoke("Activate", Location, Direction, InSpeed, TravelDistance)
    local ActualLocation = CarActor:GetActorLocation()
    print("[LevelManager] SpawnCar:", CarActor:GetName(),
        "Active:", Entry.Script:Invoke("IsActive"), "Speed:", InSpeed,
        "Location:", ActualLocation.X, ActualLocation.Y, ActualLocation.Z,
        "HasBegunPlay:", Entry.Script:HasBegunPlay(),
        "TickEnabled:", Entry.Script:IsComponentTickEnabled())
    return Entry
end

local function SpawnTraffic(DeltaSeconds)
    local Origin = FVector.new(6.0, 0.0, 0.0)

    for _, Lane in ipairs(ActiveLane) do
        -- NextSpawnTime은 다음 생성까지 남은 게임 시간(초)입니다.
        Lane.NextSpawnTime = Lane.NextSpawnTime - DeltaSeconds
        if Lane.NextSpawnTime <= 0.0 then
            print("[LevelManager] 차선 배치 시도:", Lane.Index, "Speed:", Lane.Speed,
                "Direction:", Lane.Direction, "Available:", #AvailableCars)
            local Sign = Lane.Direction and 1.0 or -1.0
            local Location = Origin + FVector.new(Lane.Index * LaneSpacing, -Sign * RoadHalfWidth, 0.0)
            local Car = SpawnCar(Location, FVector.new(0.0, Sign, 0.0), Lane.Speed, RoadHalfWidth * 2.0)

            if (Car) then
                Car.Script:Invoke("SetDirection", Lane.Direction)
            end

            Lane.NextSpawnTime = GetSpawnInterval()
        end
    end
end

function BeginPlay(Self)
    LevelManager = Self
    print("[LevelManager] BeginPlay:", Actor:GetName())

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

    print("[LevelManager] 풀 초기화 완료, Pool:", #Pool, "Available:", #AvailableCars,
        "LaneCount:", #ActiveLane)
end

function TickComponent(Self, DeltaSeconds)
    local GameState = _G.CrossyGameState

    -- 매 프레임 출력하지 않고 2초마다 배치 조건과 타이머 진행을 확인합니다.
    DebugLogElapsed = DebugLogElapsed - DeltaSeconds
    if DebugLogElapsed <= 0.0 then
        DebugLogElapsed = 2.0
        local Lane = ActiveLane[1]
        print("[LevelManager] Tick, Phase:", GameState and GameState.Phase,
            "DeltaSeconds:", DeltaSeconds, "Pool:", #Pool, "Available:", #AvailableCars,
            "FirstLaneNextSpawnTime:", Lane and Lane.NextSpawnTime)
    end

    if not GameState or GameState.Phase ~= "Playing" then
        if bWasPlaying then
            for _, Entry in ipairs(Pool) do
                Entry.Script:Invoke("Deactivate")
            end

            for _, Lane in ipairs(ActiveLane) do
                Lane.NextSpawnTime = GetSpawnInterval()
            end

            bWasPlaying = false
        end
        return
    end

    bWasPlaying = true
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
