-- X축은 진행 방향, Y축은 차량 이동 방향입니다. 위치는 매니저 액터 기준입니다.
local PoolCount = 100
local LaneSpacing = 3.0
local RoadHalfWidth = 34.0
local SpawnInterval = 5.0
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
local SpawnElapsed = 0.0
local bWasPlaying = false

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
    for _, Entry in ipairs(Pool) do
        Entry.Actor:Destroy()
    end

    Pool = {}
    SpawnElapsed = 0.0
end
