local MoveDistance = 1.0

local YawOffset = 180.0

local bWasWDown = false
local bWasSDown = false
local bWasADown = false
local bWasDDown = false

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
        local MeshComponent =
            UStaticMeshComponent.Cast(Component)

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

    if PressCount ~= 1 then
        return
    end

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

    local MeshComponent = FindStaticMeshComponent(Actor)

    if MeshComponent == nil then
        print("[PlayerControl] No UStaticMeshComponent on Actor")
    else
        local Yaw = GetYawFromDirection(MoveDirection)

        if Yaw ~= nil then
            Yaw = Yaw + YawOffset
            if Yaw > 180.0 then
                Yaw = Yaw - 360.0
            end

            MeshComponent:SetRelativeRotation(FRotator.new(0.0, Yaw, 0.0))

            local ActualRotation =  MeshComponent:GetWorldRotation()

            print("[PlayerControl] Target Yaw:", Yaw, "Actual Yaw:", ActualRotation.Yaw)
        end
    end

    local CurrentLocation = RootComponent:GetWorldLocation()

    local NewLocation =  CurrentLocation + MoveDirection * MoveDistance

    RootComponent:SetWorldLocation(NewLocation, false)
end