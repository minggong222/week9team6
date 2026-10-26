local MoveDistance = 3.0
-- 맵 매니저가 있으면 해당 맵의 이동 경계를 우선 사용합니다.
local MinY = -33
local MaxY = 33
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

-- 사망/재시작 시 진행 중인 점프까지 즉시 취소합니다.
function StopMovement()
    bIsJumping, JumpElapsed = false, 0.0
    local Mesh = FindStaticMeshComponent(Actor)
    if Mesh and MeshBaseLocation then Mesh:SetRelativeLocation(MeshBaseLocation) end
    bWasWDown = Input.IsKeyDown(EKeyCode.W) or Input.IsKeyDown(EKeyCode.Up)
    bWasSDown = Input.IsKeyDown(EKeyCode.S) or Input.IsKeyDown(EKeyCode.Down)
    bWasADown = Input.IsKeyDown(EKeyCode.A) or Input.IsKeyDown(EKeyCode.Left)
    bWasDDown = Input.IsKeyDown(EKeyCode.D) or Input.IsKeyDown(EKeyCode.Right)
end

-- 캐릭터 선택 완료 후 새 메시의 바닥 위치를 점프 기준으로 갱신합니다.
function RefreshMeshBaseLocation()
    bIsJumping, JumpElapsed = false, 0.0
    local Mesh = FindStaticMeshComponent(Actor)
    MeshBaseLocation = Mesh and Mesh:GetRelativeLocation() or nil
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
