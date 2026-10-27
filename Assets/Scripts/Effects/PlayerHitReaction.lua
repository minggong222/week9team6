-- 피격 표현과 복원 기준은 이 스크립트에서만 관리
local Config = { SquashRatio = 0.25 }
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
        local Scale = Refs.Mesh:GetRelativeScale3D()
        Scale.Z = State.Scale.Z * Config.SquashRatio
        Refs.Mesh:SetRelativeScale3D(Scale)
    end
end

function ResetPlayerPose()
    if Refs.Mesh and State.Scale then
        Refs.Mesh:SetRelativeScale3D(State.Scale)
        Refs.Mesh:SetRelativeLocation(State.Location)
        Refs.Mesh:SetRelativeRotation(State.Rotation)
    end
end
