-- 카메라 구도만 관리. 선택 시스템의 내부 상태 접근 제외
local Config = dofile("Assets/Scripts/ConfigManager.lua").Camera
State = {}
Refs = {}

function BeginPlay(Self)
    Self:SetComponentTickEnabled(false)
end

function Initialize(Camera)
    Refs.Camera = Camera
    if Camera then
        State.InitialLocation = Camera:GetWorldLocation()
        State.InitialRotation = Camera:GetWorldRotation()
    end
end

function ApplyGameplayCamera()
    local Camera = Refs.Camera
    if not Camera then return end

    local Rotation = FRotator.new(Config.Pitch, Config.Yaw, 0)
    Camera:SetRelativeRotation(Rotation)
    Camera:SetRelativeLocation(FVector.new(Config.LookAhead, 0, 0)
        - Rotation:Quaternion():GetForwardVector() * Config.Distance)
    Camera:SetIsOrthogonal(true)
    Camera:SetOrthoWidth(Config.Width)
end

-- 프리뷰 공간의 목표 위치는 GameManager에서 전달
function ShowSelectionCamera(TargetLocation)
    local Camera = Refs.Camera
    if Camera and State.InitialLocation then
        Camera:SetIsOrthogonal(true)
        Camera:SetOrthoWidth(Config.SelectionWidth)
        Camera:SetWorldLocation(TargetLocation, false)
        Camera:SetWorldRotation(State.InitialRotation, false)
    end
end

function UpdateSelectionCamera(DeltaSeconds, TargetLocation)
    local Camera = Refs.Camera
    if Camera and State.InitialLocation then
        local Current = Camera:GetWorldLocation()
        Camera:SetWorldLocation(Current + (TargetLocation - Current) * math.min(1, DeltaSeconds * Config.MoveSpeed), false)
    end
end

-- 크레딧 화면은 별도 CineCamera로 배경만 천천히 훑는다.
function StartCreditsCamera()
    local World = Actor:GetWorld()
    local ReturnCamera = Refs.Camera
    if not ReturnCamera then return nil end

    if not State.CreditsActor then
        local CineClass = UClass.Find("ACineCameraActor")
        if not CineClass then return nil end
        local Spawned = World:SpawnActor(CineClass, "CreditsCamera")
        if not Spawned then return nil end
        State.CreditsActor = ACineCameraActor.Cast(Spawned)
        if not State.CreditsActor then
            Spawned:Destroy()
            return nil
        end
    end

    local Cine = State.CreditsActor
    local Camera = Cine:GetCameraComponent()
    State.CreditsLocation = ReturnCamera:GetWorldLocation()
    State.CreditsRotation = ReturnCamera:GetWorldRotation()
    State.CreditsElapsed = 0
    Cine:SetSensorWidth(36.0)
    Cine:SetFocalLength(32.0)
    Camera:SetIsOrthogonal(false)
    Camera:SetWorldLocation(State.CreditsLocation, false)
    Camera:SetWorldRotation(State.CreditsRotation, false)
    if not World:SetMainCameraComponent(Camera, 0.0) then return nil end
    State.CreditsReturnCamera = ReturnCamera
    return Camera
end

function UpdateCreditsCamera(DeltaSeconds)
    if not State.CreditsReturnCamera then return end
    State.CreditsElapsed = State.CreditsElapsed + DeltaSeconds
    local Time = State.CreditsElapsed
    local Orientation = State.CreditsRotation:Quaternion()
    local Location = State.CreditsLocation + Orientation:GetRightVector() * (math.sin(Time * 0.12) * 8.0)
        + Orientation:GetUpVector() * (math.sin(Time * 0.08) * 1.5)
    local Camera = State.CreditsActor:GetCameraComponent()
    Camera:SetWorldLocation(Location, false)
    Camera:SetWorldRotation(FRotator.new(State.CreditsRotation.Pitch,
        State.CreditsRotation.Yaw + math.sin(Time * 0.10) * 6.0, State.CreditsRotation.Roll), false)
end

function StopCreditsCamera()
    if not State.CreditsReturnCamera then return end
    Actor:GetWorld():SetMainCameraComponent(State.CreditsReturnCamera, 0.0)
    State.CreditsReturnCamera = nil
end
