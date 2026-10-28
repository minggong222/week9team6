-- 맵·교통·기차의 생성·회수 순서 및 게임 매니저 연결 관리
local Config = dofile("Assets/Scripts/ConfigManager.lua").Level
-- 엔진이 초기화한 Lua 난수에서 실행별 시드를 얻는다. 파생 시드의 오프셋 범위도 확보한다.
local function NewRunSeed()
    return math.random(1, 2147378917)
end

State = { HighestRow = 0, Seed = NewRunSeed(), TrafficRandomState = 1, RailSlots = {} }
State.LightingProfile = Config.NightMode and "Night" or "Day"
Refs = { Lights = { Sky = {}, Sun = {}, Car = {}, Train = {} } }

local function ApplyLight(Light, Settings)
    local Color = Settings.Color
    if Settings.AttenuationRadius then Light:SetAttenuationRadius(Settings.AttenuationRadius) end
    Light:SetIntensity(Settings.Intensity)
    Light:SetLightColor(FColor.new(Color[1], Color[2], Color[3], Color[4] or 255))
end

-- 동적으로 생성한 차량 조명도 같은 설정을 적용하도록 등록한다.
function RegisterLight(Kind, Light)
    local Lights = assert(Refs.Lights[Kind], "Unknown light kind: " .. tostring(Kind))
    for _, Existing in ipairs(Lights) do if Existing == Light then return end end
    Lights[#Lights + 1] = Light
    if Kind == "Car" or Kind == "Train" then
        local Settings = Config.Spotlight
        Light:SetShadowSlopeBias(Settings.ShadowSlopeBias)
        Light:SetInnerConeAngle(Settings.InnerConeAngle)
        Light:SetOuterConeAngle(Settings.OuterConeAngle)
    end
    ApplyLight(Light, Config.Lighting[State.LightingProfile][Kind])
end

-- 조명만 전환하며 교통 난이도와 장애물의 표시 상태는 유지한다.
function SetLightingProfile(Profile)
    local Settings = assert(Config.Lighting[Profile], "Unknown lighting profile: " .. tostring(Profile))
    State.LightingProfile = Profile
    for Kind, Lights in pairs(Refs.Lights) do
        for _, Light in ipairs(Lights) do ApplyLight(Light, Settings[Kind]) end
    end
end

-- 현재 선택된 낮/밤 설정의 Intensity와 RGBA를 변경한다.
function SetLighting(Kind, Intensity, Red, Green, Blue, Alpha)
    assert(Config.Lighting[State.LightingProfile][Kind], "Unknown light kind: " .. tostring(Kind))
    assert(Intensity >= 0, "Light intensity must be non-negative")
    local Settings = { Intensity = Intensity, Color = { Red, Green, Blue, Alpha or 255 },
        AttenuationRadius = Config.Lighting[State.LightingProfile][Kind].AttenuationRadius }
    for _, Value in ipairs(Settings.Color) do
        assert(Value >= 0 and Value <= 255, "Light color must be within 0..255")
    end
    Config.Lighting[State.LightingProfile][Kind] = Settings
    for _, Light in ipairs(Refs.Lights[Kind]) do ApplyLight(Light, Settings) end
end

local function InitializeLighting()
    for _, Light in ipairs(Actor:GetWorld():GetLightComponents()) do
        if Light:GetLightType() == ELightType.Sky then RegisterLight("Sky", Light) end
        if Light:GetLightType() == ELightType.Directional then RegisterLight("Sun", Light) end
    end
end

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

-- 강에서 다른 지형으로 이동할 때 목표 Y 좌표를 계산한다.
-- 강에서 강으로 이동할 때는 좌표를 보정하지 않는다.
function GetRiverExitTargetY(CurrentX, TargetX, CurrentY)
    if not Refs.Layout or not Refs.Layout.HasMapPool or not Refs.Map then return nil end
    local CellSize = Refs.Layout.CellSize
    local Origin = Refs.Layout.Origin
    local CurrentIndex = math.floor((CurrentX - Origin.X) / CellSize + 0.0001)
    local TargetIndex = math.floor((TargetX - Origin.X) / CellSize + 0.0001)
    local CurrentKind = nil
    local TargetKind = nil
    for _, Row in ipairs(Refs.Map:Invoke("GetRows")) do
        if Row.Active then
            if Row.Index == CurrentIndex then CurrentKind = Row.Kind end
            if Row.Index == TargetIndex then TargetKind = Row.Kind end
            if CurrentKind ~= nil and TargetKind ~= nil then break end
        end
    end
    if CurrentKind ~= "river" or TargetKind == nil or TargetKind == "river" then return nil end
    return Origin.Y + math.floor((CurrentY - Origin.Y) / CellSize + 0.5) * CellSize
end

function GetGroundZ()
    return State.StartPosition.Z
end

function DetachPlayerFromLog()
    if Refs.River then Refs.River:Invoke("DetachPlayer") end
end

function ResolveRiverLanding(Position)
    if Refs.River then return Refs.River:Invoke("ResolveLanding", Position) end
    return false
end
-- 도로 배치와 기차 대기가 사용하던 공통 난수 스트림의 소비 순서 유지
function TrafficRandom()
    State.TrafficRandomState = State.TrafficRandomState * 48271 % 2147483647
    return State.TrafficRandomState / 2147483647
end

local function StopPlayerMovement()
    if Refs.PlayerScript then Refs.PlayerScript:Invoke("StopMovement") end
end

function RequestGameOver(Reason, HitDirection)
    if GetPhase() ~= "Playing" then return end
    if Refs.Game then
        Refs.Game:Invoke("GameOver", HitDirection)
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
    Info.ActiveRows, Info.RoadRows, Info.GrassRows, Info.RailRows, Info.RiverRows, Info.ActiveTrees = 0, 0, 0, 0, 0, 0
    Info.RoadLanes, Info.RailLanes, State.RiverLanes, State.RailSlots = {}, {}, {}, {}

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
            elseif Row.Kind == "rail" then
                local Lane = Refs.Train:Invoke("GetLaneInfo", Row.Id)
                Lane.X = Layout.Origin.X + Row.Index * Config.CellSize
                Info.RailLanes[Row.Index] = Lane
                State.RailSlots[Row.Index] = Row.Id
                Info.RailRows = Info.RailRows + 1
            elseif Row.Kind == "river" then
                Info.RiverRows = Info.RiverRows + 1
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
    elseif Kind == "river" then
        Refs.River:Invoke("SpawnRow", Id, Index)
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
    Refs.River:Invoke("RemoveRow", Id)
    Refs.Map:Invoke("RecycleRow", Id, CountReuse)
end

function ResetLevel(NewSeed)
    -- 차량 반환 -> 각 시스템의 행 정리 -> 난수 초기화 -> 행 생성 순서 유지
    Refs.Traffic:Invoke("DeactivateCars")
    for _, Row in ipairs(Refs.Map:Invoke("GetRows")) do RecycleRow(Row.Id, false) end
    if NewSeed then State.Seed = NewRunSeed() end
    State.HighestRow = 0
    State.TrafficRandomState = (State.Seed + 104729) % 2147483647
    Refs.Map:Invoke("ResetGeneration", State.Seed)
    Refs.Train:Invoke("ResetRandom", (State.Seed + 65537) % 2147483647)
    Refs.River:Invoke("ResetRandom", (State.Seed + 130363) % 2147483647)
    State.Info.ActiveTrains, State.Info.Seed = 0, State.Seed

    if Refs.Layout.HasMapPool then
        GenerateMapRows(-Config.RowsBehind, Config.RowsAhead)

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

function GetMode()
    return Refs.Layout.NightMode and "Night" or "Day"
end

-- 선택 화면에서 모드 변경 시 같은 시드로 교통을 다시 구성한다.
function SetMode(Mode)
    assert(Mode == "Day" or Mode == "Night", "Unknown mode: " .. tostring(Mode))
    if GetMode() == Mode then return end
    Refs.Layout.NightMode = Mode == "Night"
    SetLightingProfile(Mode)
    ResetLevel(false)
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
    State.Initialized = false
    InitializeLighting()
    assert(Config.DeathRowsBehind > 0 and Config.DeathRowsBehind <= Config.RowsBehind, "Death boundary must be inside the retained map")
    assert(Config.HalfWidth >= Config.CellSize and Config.HalfWidth <= 33 and Config.BoundaryWidth > 0, "Invalid map width")
    Refs.Player = Actor:GetWorld():GetPlayerPawn()
    assert(Refs.Player, "Select a Player Pawn")
    State.StartPosition = Refs.Player:GetActorLocation()
    local HasMapPool = Actor:FindComponentByName("MapFloor_1") ~= nil
    if HasMapPool then Actor:SetActorEnableCollision(true) end

    -- Layout은 공통 배치값 전달용. 각 시스템의 실행 상태는 별도 보관
    Refs.Layout = {
        NightMode = Config.NightMode, CellSize = Config.CellSize, RowsBehind = Config.RowsBehind, RowsAhead = Config.RowsAhead,
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
    Refs.River = AttachScript("CrossyRiverManager", "Scripts/World/RiverManager.lua")
    Refs.Map:Invoke("Initialize", Refs.Layout)
    local RowCount, TreeCount = Refs.Map:Invoke("GetPoolSize")
    Refs.Layout.RowCount = RowCount
    Refs.Traffic:Invoke("Initialize", Refs.Traffic, Self, Refs.Player, Refs.PlayerScript, Refs.Layout)
    Refs.Train:Invoke("Initialize", Self, Refs.Player, Refs.Layout)
    Refs.River:Invoke("Initialize", Refs.River, Self, Refs.Player, Refs.PlayerScript, Refs.Layout)
    State.Game = _G.CrossyGameState or { Phase = "Playing", Score = 0, MaxRow = 0 }
    _G.CrossyGameState = State.Game
    local Width = HasMapPool and Config.HalfWidth or 33
    State.Info = {
        Manager = Self, IsEndless = HasMapPool, CellSize = Config.CellSize, StartX = State.StartPosition.X,
        MinY = Refs.Layout.Origin.Y - Width, MaxY = Refs.Layout.Origin.Y + Width,
        PoolRows = RowCount, PoolTrees = TreeCount, ActiveTrains = 0
    }

    -- 전역 공개 정보는 매니저 연결 및 기존 콘솔 호출의 호환용으로만 사용
    State.Info.SetLightingProfile = function(Profile) Self:Invoke("SetLightingProfile", Profile) end
    State.Info.SetLighting = function(...) Self:Invoke("SetLighting", ...) end
    State.Info.Reset = function(NewSeed) Self:Invoke("ResetLevel", NewSeed) end
    State.Info.EnableTraffic = function() Self:Invoke("EnableTraffic") end
    State.Info.SetTrainSpeed = function(Speed, RowIndex) Self:Invoke("SetTrainSpeed", Speed, RowIndex) end
    _G.CrossyLevel = State.Info
    if HasMapPool then _G.CrossyMapTest = State.Info end
    if State.Game.Manager then BindGame(State.Game.Manager) end

    ResetLevel(false)
    if State.Game.EnableTraffic then EnableTraffic() end
    State.Initialized = true
end

function PlayTreeHit(BlockingComponent)
    if State.Initialized then Refs.Map:Invoke("PlayTreeHit", BlockingComponent) end
end

function TickComponent(Self, DeltaSeconds)
    if not State.Initialized then return end
    Refs.Map:Invoke("UpdateTreeHits", Actor:GetWorld():GetRealDeltaSeconds())
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

    -- 사망·크레딧 화면에서도 교통 유지, 선택·준비·클리어에서는 정지
    local Phase = GetPhase()
    if Phase == "Playing" or Phase == "Dead" or Phase == "Credits" then
        Refs.Traffic:Invoke("Update", DeltaSeconds)
        if Refs.Layout.HasMapPool then
            Refs.Train:Invoke("Update", DeltaSeconds)
            Refs.River:Invoke("Update", DeltaSeconds)
            RefreshTrainState()
        end
    end
    State.Info.Cars = Refs.Traffic:Invoke("GetCarCount")
    if Input.IsKeyPressed(EKeyCode.P) then PrintLevelState() end
end

function EndPlay(Self)
    Refs.Train:Invoke("Shutdown")
    Refs.River:Invoke("Shutdown")
    Refs.Map:Invoke("ReleaseCollisions")
    Refs.Traffic:Invoke("DestroyCars")
    if _G.CrossyLevel == State.Info then _G.CrossyLevel, _G.CrossyMapTest = nil, nil end
    if not Refs.Game and _G.CrossyGameState == State.Game then _G.CrossyGameState = nil end
end
