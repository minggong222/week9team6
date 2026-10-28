local MoveDistance = 3.0
-- 맵 매니저가 있으면 해당 맵의 이동 경계를 우선 사용
local MinY = -33
local MaxY = 33
local YawOffset = 180.0
local JumpDuration = 0.1
local JumpHeight = 0.5
local JumpSquashRatio = 0.12
local JumpSquashDuration = 0.08
local LandingSquashDuration = 0.08
local BounceDuration = 0.12 -- 나무에 부딪힌 뒤 출발 칸으로 돌아오는 시간
local CollisionStep = 0.01 -- 프레임이 길어져도 점프 이동을 나눠 충돌 검사
local TreeHitboxExtent = FVector.new(1.0, 1.0, 0.5)

-- 이동 상태와 연결 참조는 이 스크립트 소유. HotReload 시 유지
State = { ResetRequested = false }
Refs = {}
State.bWasWDown = false
State.bWasSDown = false
State.bWasADown = false
State.bWasDDown = false

State.bIsJumping = false
State.bIsReturning = false
State.JumpElapsed = 0.0
State.ReturnElapsed, State.CurrentJumpHeight, State.ReturnStartHeight = 0.0, 0.0, 0.0
State.ReturnStartLocation = FVector.new(0.0, 0.0, 0.0)
State.JumpStartLocation = FVector.new(0.0, 0.0, 0.0)
State.JumpDirection = FVector.new(0.0, 0.0, 0.0)
State.MeshBaseLocation = nil
State.MeshBaseRotation = nil
State.MeshBaseScale = nil
State.MoveQueue = {}
State.JumpSquashElapsed = 0.0
State.LandingSquashRemaining = 0.0

function BindGame(GameManager)
    Refs.Game = GameManager
end

function BindLevel(LevelManager)
    Refs.Level = LevelManager
end

function IsJumping()
    return State.bIsJumping
end

function RequestReset(Requested)
    State.ResetRequested = Requested
end

local function PublishJumping(IsJumping)
    local Manager = Refs.Game or Refs.Level
    if Manager then Manager:Invoke("SetPlayerJumping", IsJumping) end
end

function InitializeComponent(Component)
    -- 메시의 점프/회전과 분리한 지면 위 플레이어 몸통 검사
    local Hitbox = UBoxComponent.Cast(Actor:FindComponentByName("PlayerTreeHitbox"))
        or UBoxComponent.Cast(Actor:AddComponent(UClass.Find("UBoxComponent"), "PlayerTreeHitbox"))
    assert(Hitbox, "Could not create player tree hitbox")
    Hitbox:SetupAttachment(Actor:GetRootComponent())
    Hitbox:SetBoxExtent(TreeHitboxExtent)
    Hitbox:SetRelativeLocation(FVector.new(0, 0, TreeHitboxExtent.Z))
    Hitbox:SetCollisionObjectType(ECollisionChannel.Pawn)
    Hitbox:SetCollisionResponseToAllChannels(ECollisionResponse.Ignore)
    Hitbox:SetCollisionResponseToChannel(ECollisionChannel.WorldStatic, ECollisionResponse.Block)
    Hitbox:SetCollisionEnabled(ECollisionEnabled.QueryOnly)
    Actor:SetActorEnableCollision(true)
end

local function GetYawFromDirection(Direction)
    local X = Direction.X
    local Y = Direction.Y

    -- 직선 방향만 사용
    if X > 0 then
        return 0.0
    elseif X < 0 then
        return 180.0
    elseif Y > 0 then
        return 90.0
    elseif Y < 0 then
        return -90.0
    end

    return nil
end

local function FindStaticMeshComponent(TargetActor)
    if TargetActor == nil then
        return nil
    end

    local Components = TargetActor:GetComponents()

    for _, Component in ipairs(Components) do
        local MeshComponent = UStaticMeshComponent.Cast(Component)

        if MeshComponent ~= nil then
            return MeshComponent
        end
    end

    return nil
end

-- 정상 착지는 예약 입력을 유지하고, 사망·재시작은 입력까지 취소
local function ResetJump(MeshComponent, bKeepQueuedInput)
    State.bIsJumping, State.bIsReturning = false, false
    PublishJumping(false)
    State.JumpElapsed, State.ReturnElapsed, State.CurrentJumpHeight = 0.0, 0.0, 0.0
    State.JumpDirection = FVector.new(0.0, 0.0, 0.0)
    if not bKeepQueuedInput then
        State.MoveQueue = {}
    end
    State.JumpSquashElapsed = 0.0
    State.LandingSquashRemaining = 0.0

    if MeshComponent and State.MeshBaseLocation then
        MeshComponent:SetRelativeLocation(State.MeshBaseLocation)
    end
    if MeshComponent and State.MeshBaseScale then
        MeshComponent:SetRelativeScale3D(State.MeshBaseScale)
    end
end

-- 사망/재시작 시 진행 중인 점프까지 즉시 취소
function StopMovement()
    local Mesh = FindStaticMeshComponent(Actor)
    ResetJump(Mesh)
    State.bWasWDown = Input.IsKeyDown(EKeyCode.W) or Input.IsKeyDown(EKeyCode.Up)
    State.bWasSDown = Input.IsKeyDown(EKeyCode.S) or Input.IsKeyDown(EKeyCode.Down)
    State.bWasADown = Input.IsKeyDown(EKeyCode.A) or Input.IsKeyDown(EKeyCode.Left)
    State.bWasDDown = Input.IsKeyDown(EKeyCode.D) or Input.IsKeyDown(EKeyCode.Right)
end

-- 캐릭터 선택 완료 후 새 메시의 바닥 위치를 점프 기준으로 갱신
function RefreshMeshBaseLocation()
    local Mesh = FindStaticMeshComponent(Actor)
    State.MeshBaseLocation = Mesh and Mesh:GetRelativeLocation() or nil
    State.MeshBaseScale = Mesh and Mesh:GetRelativeScale3D() or nil
    ResetJump(Mesh)
end

function TickComponent(Component, DeltaSeconds)

    -- HotReload는 기존 State를 유지하므로 새로 추가된 필드만 보충한다.
    State.MoveQueue = State.MoveQueue or {}
    State.JumpSquashElapsed = State.JumpSquashElapsed or 0.0
    State.LandingSquashRemaining = State.LandingSquashRemaining or 0.0
    -- 이전 FIFO 버전에서 HotReload해도 가장 최근 예약 한 칸만 유지한다.
    if #State.MoveQueue > 1 then
        State.MoveQueue = { State.MoveQueue[#State.MoveQueue] }
        State.JumpSquashElapsed = 0.0
    end

    if Actor == nil then
        return
    end

    local RootComponent = Actor:GetRootComponent()

    if RootComponent == nil then
        return
    end

    local MeshComponent = FindStaticMeshComponent(Actor)

    if MeshComponent ~= nil and State.MeshBaseLocation == nil then
        State.MeshBaseLocation = MeshComponent:GetRelativeLocation()
        State.MeshBaseRotation = MeshComponent:GetRelativeRotation()
    end

    local bWDown = Input.IsKeyDown(EKeyCode.W) or Input.IsKeyDown(EKeyCode.Up)
    local bSDown = Input.IsKeyDown(EKeyCode.S) or Input.IsKeyDown(EKeyCode.Down)
    local bADown = Input.IsKeyDown(EKeyCode.A) or Input.IsKeyDown(EKeyCode.Left)
    local bDDown = Input.IsKeyDown(EKeyCode.D) or Input.IsKeyDown(EKeyCode.Right)

    local bWPressed = bWDown and not State.bWasWDown
    local bSPressed = bSDown and not State.bWasSDown
    local bAPressed = bADown and not State.bWasADown
    local bDPressed = bDDown and not State.bWasDDown

    State.bWasWDown = bWDown
    State.bWasSDown = bSDown
    State.bWasADown = bADown
    State.bWasDDown = bDDown

    local Manager = Refs.Game or Refs.Level
    local Phase = Manager and Manager:Invoke("GetPhase")

    -- 사망 중 HotReload한 경우 피격 압축을 기본 크기로 저장하지 않는다.
    if MeshComponent and State.MeshBaseScale == nil and Phase == "Playing" then
        State.MeshBaseScale = MeshComponent:GetRelativeScale3D()
    end

    -- 재시작 시 이전 점프를 취소하고 시작 방향 복원
    if State.ResetRequested then
        State.ResetRequested = false
        if Manager then Manager:Invoke("AcknowledgePlayerReset") end

        ResetJump(MeshComponent)

        if MeshComponent and State.MeshBaseRotation then
            MeshComponent:SetRelativeRotation(State.MeshBaseRotation)
        end

        return
    end

    -- 사망·클리어·캐릭터 선택 중에는 점프도 중단
    if Phase ~= "Playing" then
        if State.bIsJumping or #State.MoveQueue > 0 or State.LandingSquashRemaining > 0.0 then
            ResetJump(MeshComponent)
        end

        return
    end

    State.LandingSquashRemaining = math.max(0.0, State.LandingSquashRemaining - DeltaSeconds)

    -- 점프 중에도 다음 한 칸만 예약하며, 새 입력은 이전 예약을 교체한다.
    if bWPressed then
        State.MoveQueue[1] = { Direction = FVector.new(1.0, 0.0, 0.0), Released = false }
    end
    if bSPressed then
        State.MoveQueue[1] = { Direction = FVector.new(-1.0, 0.0, 0.0), Released = false }
    end
    if bAPressed then
        State.MoveQueue[1] = { Direction = FVector.new(0.0, -1.0, 0.0), Released = false }
    end
    if bDPressed then
        State.MoveQueue[1] = { Direction = FVector.new(0.0, 1.0, 0.0), Released = false }
    end
    if bWPressed or bSPressed or bAPressed or bDPressed then
        State.JumpSquashElapsed = 0.0
    end

    -- 예약 키의 해제 여부는 착지 전에도 기록한다.
    for _, Move in ipairs(State.MoveQueue) do
        local MoveDirection = Move.Direction
        local bDirectionDown = (MoveDirection.X > 0.0 and bWDown)
            or (MoveDirection.X < 0.0 and bSDown)
            or (MoveDirection.Y < 0.0 and bADown)
            or (MoveDirection.Y > 0.0 and bDDown)

        if not bDirectionDown then Move.Released = true end
    end

    -- 지상에서 누르고 있으면 압축, 키를 떼면 예약 한 칸을 소비한다.
    local ReleasedMoveDirection = nil
    if not State.bIsJumping and State.MoveQueue[1] then
        if State.MoveQueue[1].Released then
            ReleasedMoveDirection = State.MoveQueue[1].Direction
            State.MoveQueue[1] = nil
            State.JumpSquashElapsed = 0.0
        else
            State.JumpSquashElapsed = math.min(JumpSquashDuration, State.JumpSquashElapsed + DeltaSeconds)
        end
    end

    if ReleasedMoveDirection ~= nil then
        local MoveDirection = ReleasedMoveDirection
        local CurrentLocation = RootComponent:GetWorldLocation()
        local bWithinBounds = true

        if MoveDirection.Y ~= 0.0 then
            local TargetY = CurrentLocation.Y + MoveDirection.Y * MoveDistance
            local AllowedMinY, AllowedMaxY = MinY, MaxY
            if Refs.Level then AllowedMinY, AllowedMaxY = Refs.Level:Invoke("GetMovementBounds") end
            bWithinBounds = TargetY >= AllowedMinY and TargetY <= AllowedMaxY
        end

        if bWithinBounds then
            State.JumpStartLocation = RootComponent:GetWorldLocation()
            State.JumpDirection = MoveDirection
            State.JumpElapsed = 0.0
            State.bIsJumping = true
            State.LandingSquashRemaining = 0.0
            PublishJumping(true)
            Audio.PlaySFX("Audio/Jump2.wav")

            if MeshComponent ~= nil then
                local Yaw = GetYawFromDirection(MoveDirection)

                if Yaw ~= nil then
                    Yaw = Yaw + YawOffset

                    if Yaw > 180.0 then
                        Yaw = Yaw - 360.0
                    end

                    MeshComponent:SetRelativeRotation(FRotator.new(0.0, Yaw, 0.0))
                end
            end
        end
    end


    -- 전진 점프 -> 충돌 시 복귀 -> 출발 칸 착지. 예약 이동은 착지 후 처리
    local Remaining = DeltaSeconds
    while State.bIsJumping and Remaining > 0 do
        local Step = math.min(Remaining, CollisionStep)
        Remaining = Remaining - Step
        if State.bIsReturning then
            State.ReturnElapsed = State.ReturnElapsed + Step
            local Alpha = math.min(1.0, State.ReturnElapsed / BounceDuration)
            if Alpha > 1.0 - 0.000001 then Alpha = 1.0 end
            local Location = State.ReturnStartLocation + (State.JumpStartLocation - State.ReturnStartLocation) * Alpha
            if Alpha >= 1.0 then Location = State.JumpStartLocation end
            if not RootComponent:TryMoveWithBlocking(Location - RootComponent:GetWorldLocation(), RootComponent:GetWorldRotation()) then
                ResetJump(MeshComponent)
                return
            end
            State.CurrentJumpHeight = State.ReturnStartHeight * (1.0 - Alpha)
            if Alpha >= 1.0 then
                ResetJump(MeshComponent, true)
                State.LandingSquashRemaining = LandingSquashDuration
            end
        else
            State.JumpElapsed = State.JumpElapsed + Step
            local Alpha = math.min(1.0, State.JumpElapsed / JumpDuration)
            if Alpha > 1.0 - 0.000001 then Alpha = 1.0 end
            local Location = State.JumpStartLocation + State.JumpDirection * (MoveDistance * Alpha)
            if RootComponent:TryMoveWithBlocking(Location - RootComponent:GetWorldLocation(), RootComponent:GetWorldRotation()) then
                State.CurrentJumpHeight = 4.0 * JumpHeight * Alpha * (1.0 - Alpha)
                if Alpha >= 1.0 then
                    ResetJump(MeshComponent, true)
                    State.LandingSquashRemaining = LandingSquashDuration
                end
            else
                State.bIsReturning, State.ReturnElapsed = true, 0.0
                State.ReturnStartLocation = RootComponent:GetWorldLocation()
                State.ReturnStartHeight = State.CurrentJumpHeight
            end
        end
        if MeshComponent and State.MeshBaseLocation then
            MeshComponent:SetRelativeLocation(State.MeshBaseLocation + FVector.new(0.0, 0.0, State.CurrentJumpHeight))
        end
    end

    -- 키다운·착지 압축이 겹쳐도 최대 비율을 넘거나 누적되지 않도록 함
    if MeshComponent and State.MeshBaseScale then
        local Alpha = State.JumpSquashElapsed / JumpSquashDuration
        local Squash = JumpSquashRatio * Alpha * (2.0 - Alpha)
        local LandingAlpha = State.LandingSquashRemaining / LandingSquashDuration
        Squash = math.max(Squash, JumpSquashRatio * LandingAlpha * LandingAlpha)
        local Width = 1.0 + Squash * 0.5
        local Scale = State.MeshBaseScale
        MeshComponent:SetRelativeScale3D(FVector.new(Scale.X * Width, Scale.Y * Width, Scale.Z * (1.0 - Squash)))
    end
end
