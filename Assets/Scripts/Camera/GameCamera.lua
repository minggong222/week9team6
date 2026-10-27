-- 카메라 구도만 관리. 선택 시스템의 내부 상태 접근 제외
local Config = { Pitch = 40.0, Yaw = -35.0, Width = 36.0, Distance = 32.0, LookAhead = 5.0, MoveSpeed = 8.0 }
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
        Camera:SetOrthoWidth(12.0)
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
