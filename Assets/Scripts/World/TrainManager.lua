-- 레일별 기차·경고등 상태 소유. 지형·차량 시스템의 직접 호출 제외
local Config = dofile("Assets/Scripts/ConfigManager.lua").Train
State = { Slots = {}, RandomState = 1, ActiveCount = 0 }
Refs = {}

local function GetDifficulty()
    return Refs.Layout.NightMode and Config.Night or Config.Day
end

function BeginPlay(Self)
    Self:SetComponentTickEnabled(false)
end

local function TrafficRandom()
    return Refs.Level:Invoke("TrafficRandom")
end

local function LoadSignalAssets()
    Refs.SignalOffMesh = UAssetManager:GetAssetByPath("Models/Crossy/Signals/crossy-rail-signal-off.obj")
    Refs.SignalOnMesh = UAssetManager:GetAssetByPath("Models/Crossy/Signals/crossy-rail-signal-on.obj")
    assert(Refs.SignalOffMesh and Refs.SignalOnMesh, "Rail signal meshes are missing; restart the editor to import assets")
end

function Initialize(Level, Player, Layout)
    Refs.Level, Refs.Player, Refs.Layout = Level, Player, Layout
    for _, Settings in ipairs({ Config.Day, Config.Night }) do
        assert(Settings.Speed > 0 and Settings.WaitMin >= 0 and Settings.WaitMax >= Settings.WaitMin,
            "Invalid train settings")
        assert(Settings.WarningTime > 0 and Settings.InitialWaitMin >= Settings.WarningTime
            and Settings.InitialWaitMax >= Settings.InitialWaitMin, "Invalid train warning settings")
    end
    assert(Config.BlinkDuration > 0 and Config.BlinkInterval > 0
        and Config.SignalSpacing > 0 and Config.SignalIntensity >= 0, "Invalid rail signal settings")
    if not Layout.HasMapPool then return end

    -- 사용하지 않는 슬롯에 남지 않도록 저장된 미리보기의 경고등도 먼저 숨김
    for _, Component in ipairs(Actor:GetComponents()) do
        if Component:GetName():find("^RailSignal_") then
            Component:SetVisibility(false)
            local Light = UPointLightComponent.Cast(Component)
            if Light then Light:SetIntensity(0) end
        end
    end
    LoadSignalAssets()
    Refs.Types = {}
    for Index, Type in ipairs(Config.Types) do
        local Mesh = UAssetManager:GetAssetByPath(Type.Path)
        assert(Mesh, "Missing train: " .. Type.Path)
        Refs.Types[Index] = { Name = Type.Name, Mesh = Mesh }
    end

    for Id = 1, Layout.RowCount do
        local Mesh = UStaticMeshComponent.Cast(Actor:FindComponentByName("MapTrain_" .. Id))
        assert(Mesh, "Map pool is incomplete; regenerate the scene")
        Mesh:SetVisibility(false, true)
        State.Slots[Id] = { Slot = Id, Train = Mesh, Active = false, TrainActive = false }
    end
end

local function SetRailSignals(Row, Visible, On)
    Row.SignalOn = Visible and On
    for _, Signal in ipairs(Row.Signals or {}) do
        Signal.Mesh:SetVisibility(Visible)
        if Signal.On ~= Row.SignalOn then
            Signal.Mesh:SetStaticMesh(Row.SignalOn and Refs.SignalOnMesh or Refs.SignalOffMesh)
            Signal.On = Row.SignalOn
        end
        Signal.Light:SetVisibility(Row.SignalOn)
        Signal.Light:SetIntensity(Row.SignalOn and Config.SignalIntensity or 0)
    end
end

local function PlaceRailSignals(Row)
    Row.Signals = Row.Signals or {}
    local HalfCount = math.floor(Refs.Layout.HalfWidth / Config.SignalSpacing)
    for Index = 1, HalfCount * 2 + 1 do
        local Signal = Row.Signals[Index]
        if not Signal then
            local Name = "RailSignal_" .. Row.Slot .. "_" .. Index
            local Mesh = UStaticMeshComponent.Cast(Actor:FindComponentByName(Name))
                or UStaticMeshComponent.Cast(Actor:AddComponent(UClass.Find("UStaticMeshComponent"), Name))
            local Light = UPointLightComponent.Cast(Actor:FindComponentByName(Name .. "_Light"))
                or UPointLightComponent.Cast(Actor:AddComponent(UClass.Find("UPointLightComponent"), Name .. "_Light"))
            assert(Mesh and Light, "Could not create rail signal")
            Mesh:SetupAttachment(Actor:GetRootComponent())
            Mesh:SetStaticMesh(Refs.SignalOffMesh)
            Mesh:SetRelativeRotation(FRotator.new(0, 270, 0))
            Light:SetupAttachment(Mesh)
            Light:SetRelativeLocation(FVector.new(0, -0.24, 2.23))
            Light:SetLightColor(FColor.new(255, 24, 12, 255))
            Light:SetAttenuationRadius(2.5)
            Light:SetCastShadows(false)
            Signal = { Mesh = Mesh, Light = Light, On = false }
            Row.Signals[Index] = Signal
        end
        Signal.Mesh:SetRelativeLocation(FVector.new(Row.Index * Refs.Layout.CellSize + Config.SignalOffsetX,
            (Index - HalfCount - 1) * Config.SignalSpacing + Refs.Layout.CellSize / 2, 0))
    end
    SetRailSignals(Row, true, false)
end

local function SpawnTrain(Row)
    State.RandomState = State.RandomState * 48271 % 2147483647
    Row.TrainType = Refs.Types[math.floor(State.RandomState / 2147483647 * #Refs.Types) + 1]
    Row.Train:SetStaticMesh(Row.TrainType.Mesh)
    Row.Train:SetRelativeRotation(FRotator.new(0, 0, 0))
    local Bounds = Row.Train:CalcBounds()
    Row.TrainHalfLength = (Bounds.Max.Y - Bounds.Min.Y) / 2
    Row.Speed, Row.TrainActive = (State.SpeedOverride or GetDifficulty().Speed), false
    local Settings = GetDifficulty()
    Row.TrainWait = Settings.InitialWaitMin + TrafficRandom() * (Settings.InitialWaitMax - Settings.InitialWaitMin)
    Row.WarningElapsed, Row.TrainWarning = 0, false
    local Sign = Row.Direction and 1 or -1
    Row.Train:SetRelativeRotation(FRotator.new(0, Row.Direction and 0 or 180, 0))
    Row.Train:SetRelativeLocation(FVector.new(Row.Index * Refs.Layout.CellSize,
        -Sign * (Refs.Layout.RoadHalfWidth + Row.TrainHalfLength + Refs.Layout.CellSize), 0))
    do -- 낮에도 조명을 생성해 모드 전환 시 재사용한다.
        Row.Headlights = Row.Headlights or {}
        for Index, Side in ipairs({ -1, 1 }) do
            local Light = Row.Headlights[Index]
            if not Light then
                Light = USpotLightComponent.Cast(Actor:AddComponent(UClass.Find("USpotLightComponent"),
                    "NightTrainLight_" .. Row.Slot .. "_" .. Index))
                assert(Light, "Could not create train headlight")
                Light:SetupAttachment(Row.Train)
                Light:SetRelativeRotation(FRotator.new(Config.Headlight.Pitch, Config.Headlight.Yaw, 0))
                Refs.Level:Invoke("RegisterLight", "Train", Light)
                Light:SetAttenuationRadius(Config.Headlight.Radius)
                Light:SetCastShadows(Config.Headlight.CastShadows)
                Row.Headlights[Index] = Light
            end
            -- 열차 종류가 바뀌어도 선두의 좌우에 대칭으로 배치한다.
            Light:SetRelativeLocation(FVector.new(Side * (Bounds.Max.X - Bounds.Min.X) * Config.Headlight.SideOffsetRatio,
                Row.TrainHalfLength, Config.Headlight.Height))
            Light:SetVisibility(false)
        end
    end
    PlaceRailSignals(Row)
end

function ResetRandom(Seed)
    State.RandomState, State.ActiveCount = Seed, 0
end

function SpawnRail(Id, Index, Direction)
    local Row = State.Slots[Id]
    Row.Index, Row.Kind, Row.Direction, Row.Active = Index, "rail", Direction, true
    SpawnTrain(Row)
end

function RemoveRow(Id)
    local Row = State.Slots[Id]
    if not Row then return end
    Row.Active, Row.TrainActive = false, false
    Row.TrainWarning, Row.WarningElapsed = false, 0
    SetRailSignals(Row, false, false)
    Row.Train:SetVisibility(false, true)
    Row.TrainType, Row.Direction, Row.Speed = nil, nil, nil
end

function Update(DeltaSeconds)
    local ActiveCount = 0
    for _, Row in ipairs(State.Slots) do
        if Row.Active and Row.Kind == "rail" then
            local Step = DeltaSeconds
            if not Row.TrainActive then
                Row.TrainWait = Row.TrainWait - DeltaSeconds
                Row.WarningElapsed = math.max(0, GetDifficulty().WarningTime - Row.TrainWait)
                if Row.TrainWait <= 0 then
                    -- 출발 대기 시간을 초과한 만큼만 이번 프레임의 이동에 반영
                    Step, Row.TrainActive = -Row.TrainWait, true
                    Row.PassSoundPlayed = false
                    Row.Train:SetVisibility(true, true)
                    for _, Light in ipairs(Row.Headlights or {}) do Light:SetVisibility(true) end
                end
            else
                Row.WarningElapsed = Row.WarningElapsed + DeltaSeconds
            end
            if Row.TrainActive then
                local Sign = Row.Direction and 1 or -1
                local Limit = Refs.Layout.RoadHalfWidth + Row.TrainHalfLength + Refs.Layout.CellSize
                local Position = Row.Train:GetRelativeLocation()
                local NextY = math.max(-Limit, math.min(Limit, Position.Y + Sign * Row.Speed * Step))
                local Bounds, DeltaY = Row.Train:CalcBounds(), NextY - Position.Y
                local PlayerLocation = Refs.Player:GetActorLocation()
                -- 플레이어 근처를 지나는 순간만 재생. 빠른 이동은 이번 프레임의 이동 구간으로 확인한다.
                if not Row.PassSoundPlayed
                    and math.min(Bounds.Min.Y, Bounds.Min.Y + DeltaY) <= PlayerLocation.Y + Config.PassSoundLead
                    and math.max(Bounds.Max.Y, Bounds.Max.Y + DeltaY) >= PlayerLocation.Y - Config.PassSoundLead then
                    Row.PassSoundPlayed = true
                    local Distance = math.abs((Bounds.Min.X + Bounds.Max.X) * 0.5 - PlayerLocation.X)
                    if Distance < Config.PassSoundRange then
                        local Attenuation = 1 - Distance / Config.PassSoundRange
                        Audio.PlaySFX("Audio/Train_pass.wav", Config.PassSoundVolume * Attenuation * Attenuation)
                    end
                end
                -- 시각적인 메시 압축·확대와 무관하게 자동차와 같은 몸통 Hitbox를 사용한다.
                local Hitbox = UBoxComponent.Cast(Refs.Player:FindComponentByName("PlayerTreeHitbox"))
                local HasBounds, PlayerBounds
                if Hitbox then
                    HasBounds, PlayerBounds = true, Hitbox:CalcBounds()
                else
                    HasBounds, PlayerBounds = Refs.Player:TryGetActorBounds()
                end

                -- 빠른 기차의 한 프레임 내 통과도 감지하도록 이동 구간 전체로 충돌 판정
                local bHit = Refs.Level:Invoke("GetPhase") == "Playing" and HasBounds
                    and Bounds.Min.X <= PlayerBounds.Max.X and Bounds.Max.X >= PlayerBounds.Min.X
                    and Bounds.Min.Z <= PlayerBounds.Max.Z and Bounds.Max.Z >= PlayerBounds.Min.Z
                    and math.min(Bounds.Min.Y, Bounds.Min.Y + DeltaY) <= PlayerBounds.Max.Y
                    and math.max(Bounds.Max.Y, Bounds.Max.Y + DeltaY) >= PlayerBounds.Min.Y

                Position.Y = NextY
                Row.Train:SetRelativeLocation(Position)
                if bHit then Refs.Level:Invoke("RequestGameOver", "기차와 충돌했습니다.", FVector.new(0, Sign, 0)) end

                if Sign * NextY >= Limit then
                    Row.TrainActive = false
                    Row.Train:SetVisibility(false, true)
                    Position.Y = -Sign * Limit
                    Row.Train:SetRelativeLocation(Position)
                    Row.TrainWait = math.max(GetDifficulty().WarningTime,
                        GetDifficulty().WaitMin + TrafficRandom() * (GetDifficulty().WaitMax - GetDifficulty().WaitMin))
                    Row.WarningElapsed = 0
                else
                    ActiveCount = ActiveCount + 1
                end
            end

            local WasWarning = Row.TrainWarning
            Row.TrainWarning = (Row.TrainActive or Row.TrainWait <= GetDifficulty().WarningTime)
                and Row.WarningElapsed < Config.BlinkDuration
            if Row.TrainWarning and not WasWarning then
                local Bounds = Row.Train:CalcBounds()
                local Distance = math.abs((Bounds.Min.X + Bounds.Max.X) * 0.5 - Refs.Player:GetActorLocation().X)
                if Distance < Config.PassSoundRange then
                    local Attenuation = 1 - Distance / Config.PassSoundRange
                    Audio.PlaySFX("Audio/bell_deng_ge_deng_v2.wav", Config.WarningSoundVolume * Attenuation * Attenuation)
                end
            end
            local On = Row.TrainWarning and math.floor(Row.WarningElapsed / Config.BlinkInterval) % 2 == 0
            SetRailSignals(Row, true, On)
        end
    end
    State.ActiveCount = ActiveCount
end

-- 전체 속도 변경값을 실행 상태에 보관해 HotReload 후에도 유지
function SetTrainSpeed(Speed, RowIndex)
    assert(Speed > 0, "Train speed must be positive")
    if RowIndex == nil then State.SpeedOverride = Speed end
    for _, Row in ipairs(State.Slots) do
        if Row.Active and (RowIndex == nil or Row.Index == RowIndex) then Row.Speed = Speed end
    end
end

function GetActiveCount()
    return State.ActiveCount
end

function Shutdown()
    for _, Row in ipairs(State.Slots) do
        SetRailSignals(Row, false, false)
    end
end

function GetLaneInfo(Id)
    local Row = State.Slots[Id]
    return {
        Index = Row.Index, Direction = Row.Direction, Speed = Row.Speed,
        TrainType = Row.TrainType and Row.TrainType.Name or nil,
        TrainActive = Row.TrainActive, Warning = Row.TrainWarning, SignalOn = Row.SignalOn,
        SecondsUntilSpawn = Row.TrainActive and 0 or math.max(0, Row.TrainWait)
    }
end
