local MoveDistance = 3.0
-- 맵 매니저가 있으면 해당 맵의 이동 경계를 우선 사용
local MinY = -33
local MaxY = 33
local YawOffset = 180.0
local JumpDuration = 0.1
local JumpHeight = 0.5
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

-- 진행 중인 점프 취소 및 메시 위치 복원
local function ResetJump(MeshComponent)
    State.bIsJumping, State.bIsReturning = false, false
    PublishJumping(false)
    State.JumpElapsed, State.ReturnElapsed, State.CurrentJumpHeight = 0.0, 0.0, 0.0
    State.JumpDirection = FVector.new(0.0, 0.0, 0.0)

    if MeshComponent and State.MeshBaseLocation then
        MeshComponent:SetRelativeLocation(State.MeshBaseLocation)
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
    ResetJump(Mesh)
end

function TickComponent(Component, DeltaSeconds)

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

    local PressCount = 0

    if bWPressed then
        PressCount = PressCount + 1
    end

    if bSPressed then
        PressCount = PressCount + 1
    end

    if bAPressed then
        PressCount = PressCount + 1
    end

    if bDPressed then
        PressCount = PressCount + 1
    end

    local Manager = Refs.Game or Refs.Level
    local Phase = Manager and Manager:Invoke("GetPhase")

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
        if State.bIsJumping then
            ResetJump(MeshComponent)
        end

        return
    end

    if not State.bIsJumping and PressCount == 1 then

        local MoveDirection = FVector.new(0.0, 0.0, 0.0)

        if bWPressed then
            MoveDirection = FVector.new(1.0, 0.0, 0.0)
        elseif bSPressed then
            MoveDirection = FVector.new(-1.0, 0.0, 0.0)
        elseif bAPressed then
            MoveDirection = FVector.new(0.0, -1.0, 0.0)
        elseif bDPressed then
            MoveDirection = FVector.new(0.0, 1.0, 0.0)
        end

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
            PublishJumping(true)
            Audio.PlaySFX("Audio/Move.wav")

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


    -- 전진 점프 -> 충돌 시 복귀 -> 출발 칸 착지. 복귀 중 새 이동 입력 차단
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
            if Alpha >= 1.0 then ResetJump(MeshComponent) end
        else
            State.JumpElapsed = State.JumpElapsed + Step
            local Alpha = math.min(1.0, State.JumpElapsed / JumpDuration)
            if Alpha > 1.0 - 0.000001 then Alpha = 1.0 end
            local Location = State.JumpStartLocation + State.JumpDirection * (MoveDistance * Alpha)
            if RootComponent:TryMoveWithBlocking(Location - RootComponent:GetWorldLocation(), RootComponent:GetWorldRotation()) then
                State.CurrentJumpHeight = 4.0 * JumpHeight * Alpha * (1.0 - Alpha)
                if Alpha >= 1.0 then ResetJump(MeshComponent) end
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
end
