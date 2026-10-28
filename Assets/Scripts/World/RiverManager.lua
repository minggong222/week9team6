local AllConfig = dofile("Assets/Scripts/ConfigManager.lua")
local Config = AllConfig.River

State = {
    Slots = {},
    LanesByIndex = {},
    RandomState = 1,
    RidingLog = nil,
    ElapsedTime = 0
}

Refs = {}

function BeginPlay(Self)
    Self:SetComponentTickEnabled(false)
end

local function RiverRandom()
    State.RandomState =
        (State.RandomState * 48271) % 2147483647

    return State.RandomState / 2147483647
end

local function RiverInteger(Min, Max)
    return Min + math.floor(
        RiverRandom() * (Max - Min + 1)
    )
end

local function CreateLogComponent(Slot, Index)
    local Name = "RiverLog_" .. Slot .. "_" .. Index

    local Component = UStaticMeshComponent.Cast(
        Actor:FindComponentByName(Name)
    ) or UStaticMeshComponent.Cast(
        Actor:AddComponent(
            UClass.Find("UStaticMeshComponent"),
            Name
        )
    )

    assert(Component, "Could not create river log")

    Component:SetupAttachment(Actor:GetRootComponent())
    Component:SetStaticMesh(Refs.LogMesh)

    -- 통나무 메시에는 충돌을 걸지 않는다.
    -- 탑승 여부는 별도 수학 계산으로 판정한다.
    Component:SetCollisionEnabled(
        ECollisionEnabled.NoCollision
    )

    Component:SetRelativeRotation(
        FRotator.new(0, Config.LogYaw, 0)
    )

    Component:SetRelativeScale3D(FVector.new(
        Config.LogScale.X,
        Config.LogScale.Y,
        Config.LogScale.Z
    ))

    Component:SetVisibility(false, true)

    return Component
end

function Initialize(Self, Level, Player, PlayerScript, Layout)
    Refs.Self = Self
    Refs.Level = Level
    Refs.Player = Player
    Refs.PlayerScript = PlayerScript
    Refs.Layout = Layout

    State.Slots = {}
    State.LanesByIndex = {}
    State.RidingLog = nil

    -- 고정 맵 테스트에서는 동적 행 풀을 사용하지 않는다.
    if not Layout.HasMapPool then
        return
    end

    Refs.LogMesh = UAssetManager:GetAssetByPath(
        Config.LogMeshPath
    )

    assert(Refs.LogMesh, "River log mesh is missing")

    assert(
        Config.LogsPerRow >= 1
        and Config.LogsPerRow == math.floor(Config.LogsPerRow),
        "Invalid logs per row"
    )

    assert(
        Config.MinSpeed > 0
        and Config.MaxSpeed >= Config.MinSpeed,
        "Invalid river speed range"
    )

    assert(
        Config.LogHalfLength > 0
        and Config.PlayerHalfWidth >= 0,
        "Invalid river collision dimensions"
    )

    -- 각 맵 슬롯이 재사용될 때 통나무도 재사용한다.
    for Slot = 1, Layout.RowCount do
        local Lane = {
            Slot = Slot,
            Index = nil,
            Active = false,
            Direction = 1,
            Speed = 0,
            Logs = {}
        }

        for Index = 1, Config.LogsPerRow do
            local Component = CreateLogComponent(
                Slot,
                Index
            )

            Lane.Logs[#Lane.Logs + 1] = {
                Component = Component,
                Lane = Lane,
                Active = false,
                DeltaY = 0
            }
        end

        State.Slots[Slot] = Lane
    end
end

function ResetRandom(Seed)
    State.RandomState = Seed % 2147483647

    if State.RandomState <= 0 then
        State.RandomState = 1
    end

    State.RidingLog = nil
end

function SpawnRow(Id, Index)
    local Lane = assert(
        State.Slots[Id],
        "Invalid river slot: " .. tostring(Id)
    )

    -- 기존 인덱스 등록을 먼저 해제한다.
    if Lane.Index ~= nil then
        if State.LanesByIndex[Lane.Index] == Lane then
            State.LanesByIndex[Lane.Index] = nil
        end
    end

    Lane.Index = Index
    Lane.Active = true

    Lane.Direction =
        RiverRandom() < 0.5 and -1 or 1

    Lane.Speed = Config.MinSpeed
        + RiverRandom()
        * (Config.MaxSpeed - Config.MinSpeed)

    State.LanesByIndex[Index] = Lane

    local HalfWidth = Refs.Layout.HalfWidth
    local Spacing = (HalfWidth * 2) / #Lane.Logs

    for LogIndex, Log in ipairs(Lane.Logs) do
        local Y = -HalfWidth
            + Spacing * (LogIndex - 0.5)

        Log.Active = true
        Log.DeltaY = 0
        Log.FloatPhase = RiverRandom() * math.pi * 2

        Log.Component:SetRelativeLocation(FVector.new(
            Index * Refs.Layout.CellSize,
            Y,
            Config.LogCenterZ
        ))

        Log.Component:SetVisibility(true, true)
    end
end

function RemoveRow(Id)
    local Lane = State.Slots[Id]
    if not Lane then return end

    if Lane.Index ~= nil then
        if State.LanesByIndex[Lane.Index] == Lane then
            State.LanesByIndex[Lane.Index] = nil
        end
    end

    -- 재사용되는 행의 통나무를 타고 있던 참조도 제거한다.
    if State.RidingLog
        and State.RidingLog.Lane == Lane then
        State.RidingLog = nil
    end

    Lane.Active = false
    Lane.Index = nil

    for _, Log in ipairs(Lane.Logs) do
        Log.Active = false
        Log.DeltaY = 0
        Log.Component:SetVisibility(false, true)
    end
end

function DetachPlayer()
    State.RidingLog = nil
end

-- 점프가 끝나 플레이어가 착지했을 때 호출한다.
function ResolveLanding(Position)
    if not Refs.Player or not Position then
        return false
    end

    if Refs.Level:Invoke("GetPhase") ~= "Playing" then
        State.RidingLog = nil
        return false
    end

    local Root = Refs.Player:GetRootComponent()

    local RowIndex = math.floor(
        (Position.X - Refs.Layout.Origin.X)
        / Refs.Layout.CellSize + 0.0001
    )

    local Lane = State.LanesByIndex[RowIndex]
    local GroundZ = Refs.Level:Invoke("GetGroundZ")

    -- 강물이 아닌 행에 착지했다면 탑승 상태를 해제한다.
    if not Lane or not Lane.Active then
        State.RidingLog = nil

        -- 강이 아닌 지형에 착지하면 Y 좌표를 격자에 맞춘다.
        local CellSize = Refs.Layout.CellSize
        local AlignedY =
            math.floor(Position.Y / CellSize + 0.5) * CellSize

        Root:SetWorldLocation(FVector.new(
            Position.X,
            AlignedY,
            GroundZ
        ), false)

        return true
    end

    -- 플레이어 Y 위치가 통나무의 길이 안에 있는지 검사한다.
    for _, Log in ipairs(Lane.Logs) do
        if Log.Active then
            local LogLocation =
                Log.Component:GetWorldLocation()

            local DistanceY = math.abs(
                Position.Y - LogLocation.Y
            )

            if DistanceY
                <= Config.LogHalfLength
                    + Config.PlayerHalfWidth then

                State.RidingLog = Log

                -- 플레이어를 통나무 상단에 맞춘다.
                Root:SetWorldLocation(FVector.new(
                    Position.X,
                    Position.Y,
                    LogLocation.Z + Config.LogTopOffset
                ), false)

                return true
            end
        end
    end

    -- 강물 행이지만 착지한 위치에 통나무가 없다.
    State.RidingLog = nil

    Root:SetWorldLocation(FVector.new(
        Position.X,
        Position.Y,
        GroundZ
    ), false)

    Refs.Level:Invoke(
        "RequestGameOver",
        "강물에 빠졌습니다."
    )

    return false
end

function Update(DeltaSeconds)
    if not Refs.Layout or not Refs.Layout.HasMapPool then
        return
    end
    State.ElapsedTime = State.ElapsedTime + DeltaSeconds
    local Phase = Refs.Level:Invoke("GetPhase")

    for _, Lane in ipairs(State.Slots) do
        if Lane.Active then
            for _, Log in ipairs(Lane.Logs) do
                if Log.Active then
                    local Current =
                        Log.Component:GetRelativeLocation()

                    local NextY = Current.Y
                        + Lane.Direction
                        * Lane.Speed
                        * DeltaSeconds

                    local Boundary =
                        Refs.Layout.HalfWidth
                        + Config.LogHalfLength

                    local bWrapped = false

                    -- 통나무가 화면 바깥으로 나가면
                    -- 반대쪽에서 다시 나타나게 한다.
                    if NextY > Boundary then
                        NextY = -Boundary
                        bWrapped = true
                    elseif NextY < -Boundary then
                        NextY = Boundary
                        bWrapped = true
                    end

                    local DeltaY = bWrapped
                        and 0 or (NextY - Current.Y)

                    Log.DeltaY = DeltaY

                    -- 탑승 중인 통나무가 사라지면 추락한다.
                    if bWrapped and State.RidingLog == Log then
                        State.RidingLog = nil

                        if Phase == "Playing" then
                            Refs.Level:Invoke(
                                "RequestGameOver",
                                "탑승한 통나무를 놓쳤습니다."
                            )
                        end
                    end

                   -- 시간에 따라 통나무가 물 위에서 부드럽게 움직이도록 한다.
                   local FloatOffset = math.sin(
                       State.ElapsedTime * Config.LogFloatSpeed
                       + (Log.FloatPhase or 0)
                   ) * Config.LogFloatAmplitude

                   -- 플레이어가 올라탄 통나무는 조금 가라앉는다.
                   local SinkOffset = State.RidingLog == Log
                       and Config.LogSinkOffset
                       or 0

                   local NextZ = Config.LogCenterZ
                       + FloatOffset
                       - SinkOffset

                   Log.Component:SetRelativeLocation(
                       FVector.new(
                           Current.X,
                           NextY,
                           NextZ
                       )
                   )

                    -- 탑승한 플레이어도 통나무와 함께 이동한다.
                    if State.RidingLog == Log
                        and Phase == "Playing" then

                        local Root =
                            Refs.Player:GetRootComponent()

                        local PlayerPosition =
                            Root:GetWorldLocation()

                        local NextPlayerY =
                            PlayerPosition.Y + DeltaY

                        local NextPlayerZ =
                            Log.Component:GetWorldLocation().Z
                            + Config.LogTopOffset

                        local MinY, MaxY =
                            Refs.Level:Invoke("GetMovementBounds")

                        if NextPlayerY < MinY
                            or NextPlayerY > MaxY then

                            State.RidingLog = nil

                            Refs.Level:Invoke(
                                "RequestGameOver",
                                "강물의 가장자리를 벗어났습니다."
                            )
                        else
                            Root:SetWorldLocation(FVector.new(
                                PlayerPosition.X,
                                NextPlayerY,
                                NextPlayerZ
                            ), false)
                        end
                    end
                end
            end
        end
    end
end

function Shutdown()
    State.RidingLog = nil

    for _, Lane in ipairs(State.Slots) do
        Lane.Active = false

        for _, Log in ipairs(Lane.Logs) do
            Log.Active = false
            Log.Component:SetVisibility(false, true)
        end
    end
end