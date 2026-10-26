local MoveDistance = 3.0
-- 맵 매니저가 있으면 해당 맵의 이동 경계를 우선 사용합니다.
local MinY = -33
local MaxY = 33
local YawOffset = 180.0
local JumpDuration = 0.1
local JumpHeight = 0.5
local BounceDuration = 0.12 -- 나무에 부딪힌 뒤 출발 칸으로 돌아오는 시간
local CollisionStep = 0.01 -- 프레임이 길어져도 점프 이동을 나눠 충돌 검사
local TreeHitboxExtent = FVector.new(1.0, 1.0, 0.5)

local bWasWDown = false
local bWasSDown = false
local bWasADown = false
local bWasDDown = false

local bIsJumping = false
local bIsReturning = false
local JumpElapsed = 0.0
local ReturnElapsed, CurrentJumpHeight, ReturnStartHeight = 0.0, 0.0, 0.0
local ReturnStartLocation = FVector.new(0.0, 0.0, 0.0)
local JumpStartLocation = FVector.new(0.0, 0.0, 0.0)
local JumpDirection = FVector.new(0.0, 0.0, 0.0)
local MeshBaseLocation = nil
local MeshBaseRotation = nil

function InitializeComponent(Component)
    -- 메시의 점프/회전과 분리해 지면 위 플레이어 몸통을 검사합니다.
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

-- 진행 중인 점프를 취소하고 메시 위치를 복원한다.
local function ResetJump(MeshComponent)
    bIsJumping, bIsReturning = false, false
    local State = _G.CrossyGameState
    if State then State.PlayerIsJumping = false end
    JumpElapsed, ReturnElapsed, CurrentJumpHeight = 0.0, 0.0, 0.0
    JumpDirection = FVector.new(0.0, 0.0, 0.0)

    if MeshComponent and MeshBaseLocation then
        MeshComponent:SetRelativeLocation(MeshBaseLocation)
    end
end

-- 사망/재시작 시 진행 중인 점프까지 즉시 취소합니다.
function StopMovement()
    local Mesh = FindStaticMeshComponent(Actor)
    ResetJump(Mesh)
    bWasWDown = Input.IsKeyDown(EKeyCode.W) or Input.IsKeyDown(EKeyCode.Up)
    bWasSDown = Input.IsKeyDown(EKeyCode.S) or Input.IsKeyDown(EKeyCode.Down)
    bWasADown = Input.IsKeyDown(EKeyCode.A) or Input.IsKeyDown(EKeyCode.Left)
    bWasDDown = Input.IsKeyDown(EKeyCode.D) or Input.IsKeyDown(EKeyCode.Right)
end

-- 캐릭터 선택 완료 후 새 메시의 바닥 위치를 점프 기준으로 갱신합니다.
function RefreshMeshBaseLocation()
    local Mesh = FindStaticMeshComponent(Actor)
    MeshBaseLocation = Mesh and Mesh:GetRelativeLocation() or nil
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

    if MeshComponent ~= nil and MeshBaseLocation == nil then
        MeshBaseLocation = MeshComponent:GetRelativeLocation()
        MeshBaseRotation = MeshComponent:GetRelativeRotation()
    end

    local bWDown = Input.IsKeyDown(EKeyCode.W) or Input.IsKeyDown(EKeyCode.Up)
    local bSDown = Input.IsKeyDown(EKeyCode.S) or Input.IsKeyDown(EKeyCode.Down)
    local bADown = Input.IsKeyDown(EKeyCode.A) or Input.IsKeyDown(EKeyCode.Left)
    local bDDown = Input.IsKeyDown(EKeyCode.D) or Input.IsKeyDown(EKeyCode.Right)

    local bWPressed = bWDown and not bWasWDown
    local bSPressed = bSDown and not bWasSDown
    local bAPressed = bADown and not bWasADown
    local bDPressed = bDDown and not bWasDDown

    bWasWDown = bWDown
    bWasSDown = bSDown
    bWasADown = bADown
    bWasDDown = bDDown

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

    local State = _G.CrossyGameState

    -- 재시작 시 이전 점프를 취소하고 시작 방향 복원
    if State and State.PlayerResetRequested then
        State.PlayerResetRequested = false

        ResetJump(MeshComponent)

        if MeshComponent and MeshBaseRotation then
            MeshComponent:SetRelativeRotation(MeshBaseRotation)
        end

        return
    end

    -- 사망·클리어·캐릭터 선택 중에는 점프도 중단
    if not State or State.Phase ~= "Playing" then
        if bIsJumping then
            ResetJump(MeshComponent)
        end

        return
    end

    if not bIsJumping and PressCount == 1 then

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
            local MapBounds = _G.CrossyLevel or _G.CrossyMapTest
            local AllowedMinY = MapBounds and MapBounds.MinY or MinY
            local AllowedMaxY = MapBounds and MapBounds.MaxY or MaxY
            bWithinBounds = TargetY >= AllowedMinY and TargetY <= AllowedMaxY
        end

        if bWithinBounds then
            JumpStartLocation = RootComponent:GetWorldLocation()
            JumpDirection = MoveDirection
            JumpElapsed = 0.0
            bIsJumping = true
            State.PlayerIsJumping = true
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


    -- 전진 점프 -> 충돌하면 복귀 -> 출발 칸에 착지. 복귀 중에도 새 이동은 받지 않습니다.
    local Remaining = DeltaSeconds
    while bIsJumping and Remaining > 0 do
        local Step = math.min(Remaining, CollisionStep)
        Remaining = Remaining - Step
        if bIsReturning then
            ReturnElapsed = ReturnElapsed + Step
            local Alpha = math.min(1.0, ReturnElapsed / BounceDuration)
            if Alpha > 1.0 - 0.000001 then Alpha = 1.0 end
            local Location = ReturnStartLocation + (JumpStartLocation - ReturnStartLocation) * Alpha
            if Alpha >= 1.0 then Location = JumpStartLocation end
            if not RootComponent:TryMoveWithBlocking(Location - RootComponent:GetWorldLocation(), RootComponent:GetWorldRotation()) then
                ResetJump(MeshComponent)
                return
            end
            CurrentJumpHeight = ReturnStartHeight * (1.0 - Alpha)
            if Alpha >= 1.0 then ResetJump(MeshComponent) end
        else
            JumpElapsed = JumpElapsed + Step
            local Alpha = math.min(1.0, JumpElapsed / JumpDuration)
            if Alpha > 1.0 - 0.000001 then Alpha = 1.0 end
            local Location = JumpStartLocation + JumpDirection * (MoveDistance * Alpha)
            if RootComponent:TryMoveWithBlocking(Location - RootComponent:GetWorldLocation(), RootComponent:GetWorldRotation()) then
                CurrentJumpHeight = 4.0 * JumpHeight * Alpha * (1.0 - Alpha)
                if Alpha >= 1.0 then ResetJump(MeshComponent) end
            else
                bIsReturning, ReturnElapsed = true, 0.0
                ReturnStartLocation = RootComponent:GetWorldLocation()
                ReturnStartHeight = CurrentJumpHeight
            end
        end
        if MeshComponent and MeshBaseLocation then
            MeshComponent:SetRelativeLocation(MeshBaseLocation + FVector.new(0.0, 0.0, CurrentJumpHeight))
        end
    end
end
