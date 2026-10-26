-- X축은 진행 방향, Y축은 차량 이동 방향입니다. 위치는 매니저 액터 기준입니다.
local PoolCount = 100
local LaneCount = 30
local LaneSpacing = 3.0
local RoadHalfWidth = 34.0
local SpawnInterval = 2.0
local Speed = 5.0

local Pool = {}
local SpawnElapsed = 0.0
local bWasPlaying = false

local function CreateCar(World, Class, Index)
    local Name = "Car " .. Index
    local Transform = FTransform.new()
    Transform.Location = FVector.new(0.0, 0.0, -1000.0)
    local Car = World:SpawnActor(Class, Name, Transform)
    
    -- Car Mesh 붙이기
    local StaticMeshComponent = UStaticMeshComponent.Cast(
        Car:FindComponentByName("UStaticMeshComponent")
    )
    local CarMesh = UAssetManager:GetAssetByPath("Models/Crossy/Cars/taxi.obj")
    StaticMeshComponent:SetStaticMesh(CarMesh)

    -- Lua 스크립트 붙이기
    local Component = Car:AddComponent(ULuaComponent.StaticClass(), "CarScript")
    local LuaComponent = ULuaComponent.Cast(Component)
    LuaComponent:SetScriptPath("Scripts/Car.lua")

    -- Lua 스크립트 로드
    LuaComponent:InitializeLua()
    if not LuaComponent:LoadLua() then
        Car:Destroy()
        return nil
    end

    LuaComponent:Invoke("Deactivate")
    return { Actor = Car, Script = LuaComponent }
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

    for Lane = 1, LaneCount do
        local Sign = Lane % 2 == 0 and 1.0 or -1.0
        local Location = Origin + FVector.new(Lane * LaneSpacing, -Sign * RoadHalfWidth, 0.0)
        local Car = SpawnCar(Location, FVector.new(0.0, Sign, 0.0), Speed, RoadHalfWidth * 2.0)

        if (Car) then
            Car.Script:Invoke("SetDirection", Lane % 2 == 0)
        end
    end
end

function BeginPlay(Self)
    local World = Actor:GetWorld()
    local Class = UClass.Find("AStaticMeshActor")

    for Index = 1, PoolCount do
        local Entry = CreateCar(World, Class, Index)
        if Entry then
            Pool[#Pool + 1] = Entry
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
