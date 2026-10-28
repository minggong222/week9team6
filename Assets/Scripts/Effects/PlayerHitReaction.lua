-- 피격 표현과 복원 기준은 이 스크립트에서만 관리
local Config = dofile("Assets/Scripts/ConfigManager.lua").HitReaction
State = {}
Refs = {}

function BeginPlay(Self)
    Self:SetComponentTickEnabled(false)
end

function CapturePose()
    if Refs.Mesh then
        State.Scale = Refs.Mesh:GetRelativeScale3D()
        State.Location = Refs.Mesh:GetRelativeLocation()
        State.Rotation = Refs.Mesh:GetRelativeRotation()
    end
end

function Initialize(PlayerMesh)
    Refs.Mesh = PlayerMesh
    CapturePose()
end

-- 충돌 위치 변경 없이 메시 높이만 압축
function PlayHitReaction()
    if Refs.Mesh and State.Scale then
        State.SquashStartZ = Refs.Mesh:GetRelativeScale3D().Z
        State.SquashElapsed = 0
    end
end

function Update(DeltaSeconds)
    if State.SquashElapsed == nil or not Refs.Mesh or not State.Scale then return end

    State.SquashElapsed = math.min(State.SquashElapsed + DeltaSeconds, Config.SquashDuration)
    local Alpha = State.SquashElapsed / Config.SquashDuration
    local SmoothAlpha = Alpha * Alpha * (3 - 2 * Alpha)
    local Scale = Refs.Mesh:GetRelativeScale3D()
    Scale.Z = State.SquashStartZ + (State.Scale.Z * Config.SquashRatio - State.SquashStartZ) * SmoothAlpha
    Refs.Mesh:SetRelativeScale3D(Scale)

    if Alpha >= 1 then State.SquashElapsed = nil end
end

function ResetPlayerPose()
    State.SquashElapsed = nil
    if Refs.Mesh and State.Scale then
        Refs.Mesh:SetRelativeScale3D(State.Scale)
        Refs.Mesh:SetRelativeLocation(State.Location)
        Refs.Mesh:SetRelativeRotation(State.Rotation)
    end
end
