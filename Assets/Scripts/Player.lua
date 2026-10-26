local MoveDistance = 2.0
local MinY = -16
local MaxY = 16
local YawOffset = 180.0
local JumpDuration = 0.1
local JumpHeight = 0.5

local bWasWDown = false
local bWasSDown = false
local bWasADown = false
local bWasDDown = false

local bIsJumping = false
local JumpElapsed = 0.0
local JumpStartLocation = FVector.new(0.0, 0.0, 0.0)
local JumpDirection = FVector.new(0.0, 0.0, 0.0)
local MeshBaseLocation = nil

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
    end

    local bWDown = Input.IsKeyDown(EKeyCode.W)
    local bSDown = Input.IsKeyDown(EKeyCode.S)
    local bADown = Input.IsKeyDown(EKeyCode.A)
    local bDDown = Input.IsKeyDown(EKeyCode.D)

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
    local bCanStartMove = true;

    if not State or State.Phase ~= "Playing" then
        bCanStartMove = false
    end

    if bCanStartMove and not bIsJumping and PressCount == 1 then

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
            bWithinBounds = TargetY >= MinY and TargetY <= MaxY
        end

        if bWithinBounds then
            JumpStartLocation = RootComponent:GetWorldLocation()
            JumpDirection = MoveDirection
            JumpElapsed = 0.0
            bIsJumping = true

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


    -- 매 프레임 점프 애니메이션 갱신
    if bIsJumping then

        JumpElapsed = JumpElapsed + DeltaSeconds

        local Alpha = JumpElapsed / JumpDuration

        if Alpha > 1.0 then
            Alpha = 1.0
        end

        -- 수평 이동을 부드럽게 진행
        local NewRootLocation = JumpStartLocation + JumpDirection * (MoveDistance * Alpha)

        RootComponent:SetWorldLocation(NewRootLocation, false)

        -- 포물선 형태로 메쉬가 위아래로 이동
        local HeightOffset = 4.0 * JumpHeight * Alpha * (1.0 - Alpha)

        if MeshComponent ~= nil and MeshBaseLocation ~= nil then
            MeshComponent:SetRelativeLocation(MeshBaseLocation + FVector.new(0.0, 0.0, HeightOffset))
        end

        -- 착지
        if Alpha >= 1.0 then

            RootComponent:SetWorldLocation(JumpStartLocation + JumpDirection * MoveDistance, false)

            if MeshComponent ~= nil and MeshBaseLocation ~= nil then
                MeshComponent:SetRelativeLocation(MeshBaseLocation)
            end

            bIsJumping = false
            JumpElapsed = 0.0
        end
    end
end