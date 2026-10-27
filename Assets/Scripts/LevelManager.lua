-- 맵·교통·기차의 생성·회수 순서 및 게임 매니저 연결 관리
local Config = {
    CellSize = 3.0, RowsBehind = 10, RowsAhead = 30, DeathRowsBehind = 6,
    HalfWidth = 24.0, BoundaryWidth = 0.3, RoadHalfWidth = 34.0, InitialSeed = 20261026
}
State = { HighestRow = 0, Seed = Config.InitialSeed, TrafficRandomState = 1, RailSlots = {} }
Refs = {}

local function AttachScript(Name, Path)
    local Script = ULuaComponent.Cast(Actor:FindComponentByName(Name))
        or ULuaComponent.Cast(Actor:AddComponent(ULuaComponent.StaticClass(), Name))
    assert(Script, "Could not create script component: " .. Name)

    local NeedsLoad = Script:GetScriptPath() ~= Path or not Script:GetLoaded()
    assert(Script:SetScriptPath(Path), "Could not find script: " .. Path)
    Script:InitializeLua()
    if NeedsLoad then assert(Script:LoadLua(), "Could not load script: " .. Path) end
    Script:SetComponentTickEnabled(false)
    return Script
end

function BindGame(GameManager)
    Refs.Game = GameManager
    GameManager:Invoke("BindLevel", State.Info.Manager)
    if Refs.PlayerScript then Refs.PlayerScript:Invoke("BindGame", GameManager) end
end

function GetPhase()
    if Refs.Game then return Refs.Game:Invoke("GetPhase") end
    return State.Game.Phase
end

function IsGameManaged()
    return Refs.Game ~= nil
end

function SetPlayerJumping(IsJumping)
    if Refs.Game then
        Refs.Game:Invoke("SetPlayerJumping", IsJumping)
    else
        State.Game.PlayerIsJumping = IsJumping
    end
end

function AcknowledgePlayerReset()
    if Refs.Game then
        Refs.Game:Invoke("AcknowledgePlayerReset")
    else
        State.Game.PlayerResetRequested = false
    end
end

function GetProgress()
    return State.HighestRow, Refs.Layout.HasMapPool, Config.CellSize
end

function GetMovementBounds()
    return State.Info.MinY, State.Info.MaxY
end

-- 도로 배치와 기차 대기가 사용하던 공통 난수 스트림의 소비 순서 유지
function TrafficRandom()
    State.TrafficRandomState = State.TrafficRandomState * 48271 % 2147483647
    return State.TrafficRandomState / 2147483647
end

local function StopPlayerMovement()
    if Refs.PlayerScript then Refs.PlayerScript:Invoke("StopMovement") end
end

function RequestGameOver(Reason)
    if GetPhase() ~= "Playing" then return end
    if Refs.Game then
        Refs.Game:Invoke("GameOver")
    else
        State.Game.Phase = "Dead"
        StopPlayerMovement()
    end
    if Reason then print("[Level] " .. Reason) end
end

-- 각 시스템의 상태 복사본으로 기존 CrossyLevel 공개 정보 구성
local function PublishLevelState()
    local Info, Layout = State.Info, Refs.Layout
    Info.HighestRow = State.HighestRow
    Info.ReusedRows = Refs.Map:Invoke("GetReusedRows")
    Info.MinRow, Info.MaxRow = State.HighestRow - Config.RowsBehind, State.HighestRow + Config.RowsAhead
    Info.ActiveRows, Info.RoadRows, Info.GrassRows, Info.RailRows, Info.ActiveTrees = 0, 0, 0, 0, 0
    Info.RoadLanes, Info.RailLanes, State.RailSlots = {}, {}, {}

    for _, Row in ipairs(Refs.Map:Invoke("GetRows")) do
        if Row.Active then
            Info.ActiveRows = Info.ActiveRows + 1
            Info.ActiveTrees = Info.ActiveTrees + Row.TreeCount
            if Row.Kind == "grass" then
                Info.GrassRows = Info.GrassRows + 1
            elseif Row.Kind == "road" then
                local Lane = Refs.Traffic:Invoke("GetLaneInfo", Row.Id)
                Lane.X = Layout.Origin.X + Row.Index * Config.CellSize
                Info.RoadLanes[Row.Index] = Lane
                Info.RoadRows = Info.RoadRows + 1
            else
                local Lane = Refs.Train:Invoke("GetLaneInfo", Row.Id)
                Lane.X = Layout.Origin.X + Row.Index * Config.CellSize
                Info.RailLanes[Row.Index] = Lane
                State.RailSlots[Row.Index] = Row.Id
                Info.RailRows = Info.RailRows + 1
            end
        end
    end
    Refs.Map:Invoke("UpdateBounds", State.HighestRow)
end

local function RefreshTrainState()
    State.Info.ActiveTrains = Refs.Train:Invoke("GetActiveCount")
    for Index, Id in pairs(State.RailSlots) do
        local Current = Refs.Train:Invoke("GetLaneInfo", Id)
        local Lane = State.Info.RailLanes[Index]
        Lane.TrainActive, Lane.Warning, Lane.SignalOn = Current.TrainActive, Current.Warning, Current.SignalOn
        Lane.SecondsUntilSpawn = Current.SecondsUntilSpawn
    end
end

-- 지형 한 행 생성 시 기존 순서대로 차량 배치 또는 기차 대기 연결
local function ConfigureRow(Id, Index, Kind)
    local Direction = Refs.Traffic:Invoke("PrepareRow", Id, Index, Kind)
    if Kind == "road" then
        Refs.Traffic:Invoke("PopulateRow", Id)
    elseif Kind == "rail" then
        Refs.Train:Invoke("SpawnRail", Id, Index, Direction)
    end
end

local function GenerateMapRows(MinRow, MaxRow)
    while true do
        local Id, Index, Kind = Refs.Map:Invoke("GenerateNextRow", MinRow, MaxRow)
        if not Id then break end
        ConfigureRow(Id, Index, Kind)
    end
end

local function RecycleRow(Id, CountReuse)
    Refs.Traffic:Invoke("RemoveRow", Id)
    Refs.Train:Invoke("RemoveRow", Id)
    Refs.Map:Invoke("RecycleRow", Id, CountReuse)
end

function ResetLevel(NewSeed)
    -- 차량 반환 -> 각 시스템의 행 정리 -> 난수 초기화 -> 행 생성 순서 유지
    Refs.Traffic:Invoke("DeactivateCars")
    for _, Row in ipairs(Refs.Map:Invoke("GetRows")) do RecycleRow(Row.Id, false) end
    if NewSeed then State.Seed = State.Seed + 1 end
    State.HighestRow = 0
    State.TrafficRandomState = (State.Seed + 104729) % 2147483647
    Refs.Map:Invoke("ResetGeneration", State.Seed)
    Refs.Train:Invoke("ResetRandom", (State.Seed + 65537) % 2147483647)
    State.Info.ActiveTrains, State.Info.Seed = 0, State.Seed

    if Refs.Layout.HasMapPool then
        GenerateMapRows(-Config.RowsBehind, Config.RowsAhead)
    else
        for Id = 1, Refs.Layout.RowCount do
            local Index = Refs.Map:Invoke("GetStaticRowIndex", Id)
            Refs.Map:Invoke("CreateRow", Id, Index, "road")
            ConfigureRow(Id, Index, "road")
        end
    end
    PublishLevelState()
end

-- 기존 공개 진입점 유지. 개별 차량은 TrafficManager에 직접 반환
function ReturnCar(CarActor)
    Refs.Traffic:Invoke("ReturnCar", CarActor)
end

function SpawnCar(Location, Direction, Speed, TravelDistance)
    return Refs.Traffic:Invoke("SpawnCar", Location, Direction, Speed, TravelDistance)
end

function EnableTraffic()
    Refs.Traffic:Invoke("EnableTraffic")
end

function SetTrainSpeed(Speed, RowIndex)
    Refs.Train:Invoke("SetTrainSpeed", Speed, RowIndex)
    PublishLevelState()
end

local function PrintLevelState()
    local Info = State.Info
    local Cars, PoolCount = Refs.Traffic:Invoke("GetCarCount")
    print("[Level] rows", Info.MinRow, "..", Info.MaxRow, "active", Info.ActiveRows,
        "road/grass/rail", Info.RoadRows, Info.GrassRows, Info.RailRows,
        "cars", Cars, "/", PoolCount, "trains", Info.ActiveTrains, "reused", Info.ReusedRows)
    for Index = Info.MinRow, Info.MaxRow do
        local Lane = Info.RoadLanes[Index] or Info.RailLanes[Index]
        if Lane then print("[Lane]", Index, Lane.Direction and "+Y" or "-Y", "speed", Lane.Speed, Lane.TrainType or "car") end
    end
end

function BeginPlay(Self)
    assert(Config.DeathRowsBehind > 0 and Config.DeathRowsBehind <= Config.RowsBehind, "Death boundary must be inside the retained map")
    assert(Config.HalfWidth >= Config.CellSize and Config.HalfWidth <= 33 and Config.BoundaryWidth > 0, "Invalid map width")
    Refs.Player = Actor:GetWorld():GetPlayerPawn()
    assert(Refs.Player, "Select a Player Pawn")
    State.StartPosition = Refs.Player:GetActorLocation()
    local HasMapPool = Actor:FindComponentByName("MapFloor_1") ~= nil
    if HasMapPool then Actor:SetActorEnableCollision(true) end

    -- Layout은 공통 배치값 전달용. 각 시스템의 실행 상태는 별도 보관
    Refs.Layout = {
        CellSize = Config.CellSize, RowsBehind = Config.RowsBehind, RowsAhead = Config.RowsAhead,
        HalfWidth = Config.HalfWidth, BoundaryWidth = Config.BoundaryWidth, RoadHalfWidth = Config.RoadHalfWidth,
        Origin = HasMapPool and Actor:GetActorLocation() or FVector.new(6, 0, 0), HasMapPool = HasMapPool
    }
    for _, Component in ipairs(Refs.Player:GetComponents()) do
        local Script, Mesh = ULuaComponent.Cast(Component), UStaticMeshComponent.Cast(Component)
        if Script and Script:GetScriptPath() == "Scripts/Player.lua" then Refs.PlayerScript = Script end
        if Mesh and not Refs.PlayerMesh then
            Refs.PlayerMesh = Mesh
            State.StartMeshLocation, State.StartMeshRotation, State.StartMeshScale = Mesh:GetRelativeLocation(), Mesh:GetRelativeRotation(), Mesh:GetRelativeScale3D()
        end
    end
    if Refs.PlayerScript then Refs.PlayerScript:Invoke("BindLevel", Self) end

    Refs.Map = AttachScript("CrossyMapGenerator", "Scripts/World/MapGenerator.lua")
    Refs.Traffic = AttachScript("CrossyTrafficManager", "Scripts/World/TrafficManager.lua")
    Refs.Train = AttachScript("CrossyTrainManager", "Scripts/World/TrainManager.lua")
    Refs.Map:Invoke("Initialize", Refs.Layout)
    local RowCount, TreeCount = Refs.Map:Invoke("GetPoolSize")
    Refs.Layout.RowCount = RowCount
    Refs.Traffic:Invoke("Initialize", Refs.Traffic, Self, Refs.Player, Refs.PlayerScript, Refs.Layout)
    Refs.Train:Invoke("Initialize", Self, Refs.Player, Refs.Layout)

    State.Game = _G.CrossyGameState or { Phase = "Playing", Score = 0, MaxRow = 0 }
    _G.CrossyGameState = State.Game
    local Width = HasMapPool and Config.HalfWidth or 33
    State.Info = {
        Manager = Self, IsEndless = HasMapPool, CellSize = Config.CellSize, StartX = State.StartPosition.X,
        MinY = Refs.Layout.Origin.Y - Width, MaxY = Refs.Layout.Origin.Y + Width,
        PoolRows = RowCount, PoolTrees = TreeCount, ActiveTrains = 0
    }

    -- 전역 공개 정보는 매니저 연결 및 기존 콘솔 호출의 호환용으로만 사용
    State.Info.Reset = function(NewSeed) Self:Invoke("ResetLevel", NewSeed) end
    State.Info.EnableTraffic = function() Self:Invoke("EnableTraffic") end
    State.Info.SetTrainSpeed = function(Speed, RowIndex) Self:Invoke("SetTrainSpeed", Speed, RowIndex) end
    _G.CrossyLevel = State.Info
    if HasMapPool then _G.CrossyMapTest = State.Info end
    if State.Game.Manager then BindGame(State.Game.Manager) end

    ResetLevel(false)
    if State.Game.EnableTraffic then EnableTraffic() end
end

function TickComponent(Self, DeltaSeconds)
    if State.Game.EnableTraffic and not Refs.Traffic:Invoke("IsEnabled") then EnableTraffic() end

    -- GameManager가 없는 maptest는 기존처럼 자체 재시작·점수 처리
    if not Refs.Game and Input.IsKeyPressed(EKeyCode.R) then
        StopPlayerMovement()
        Refs.Player:GetRootComponent():SetWorldLocation(State.StartPosition, false)
        if Refs.PlayerMesh then
            Refs.PlayerMesh:SetRelativeLocation(State.StartMeshLocation)
            Refs.PlayerMesh:SetRelativeRotation(State.StartMeshRotation)
            Refs.PlayerMesh:SetRelativeScale3D(State.StartMeshScale)
        end
        State.Game.Score, State.Game.MaxRow, State.Game.Phase = 0, 0, "Playing"
        ResetLevel(true)
    end

    if GetPhase() == "Playing" and Refs.Layout.HasMapPool then
        local Position = Refs.Player:GetActorLocation()
        local DeathX = Refs.Layout.Origin.X + (State.HighestRow - Config.DeathRowsBehind) * Config.CellSize
        if Position.X < DeathX - 0.0001 then
            Refs.Player:GetRootComponent():SetWorldLocation(FVector.new(DeathX, Position.Y, Position.Z), false)
            RequestGameOver("뒤쪽 사망 경계를 벗어났습니다.")
        else
            local Current = math.floor((Position.X - Refs.Layout.Origin.X) / Config.CellSize + 0.0001)
            if Current > State.HighestRow then
                State.HighestRow = Current
                for _, Row in ipairs(Refs.Map:Invoke("GetRows")) do
                    if Row.Active and Row.Index < Current - Config.RowsBehind then RecycleRow(Row.Id, true) end
                end
                GenerateMapRows(Current - Config.RowsBehind, Current + Config.RowsAhead)
                PublishLevelState()
            end
            if not Refs.Game then State.Game.MaxRow, State.Game.Score = State.HighestRow, State.HighestRow end
        end
    end

    -- 사망 후에도 교통 유지, 선택·준비·클리어에서는 정지
    local Phase = GetPhase()
    if Phase == "Playing" or Phase == "Dead" then
        Refs.Traffic:Invoke("Update", DeltaSeconds)
        if Refs.Layout.HasMapPool then
            Refs.Train:Invoke("Update", DeltaSeconds)
            RefreshTrainState()
        end
    end
    State.Info.Cars = Refs.Traffic:Invoke("GetCarCount")
    if Input.IsKeyPressed(EKeyCode.P) then PrintLevelState() end
end

function EndPlay(Self)
    Refs.Train:Invoke("Shutdown")
    Refs.Map:Invoke("ReleaseCollisions")
    Refs.Traffic:Invoke("DestroyCars")
    if _G.CrossyLevel == State.Info then _G.CrossyLevel, _G.CrossyMapTest = nil, nil end
    if not Refs.Game and _G.CrossyGameState == State.Game then _G.CrossyGameState = nil end
end
