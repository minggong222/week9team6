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

function Initialize(PlayerMesh, Player)
    Refs.Mesh = PlayerMesh
    Refs.PlayerRoot = Player and Player:GetRootComponent()
    CapturePose()
end

-- 차량 진행 방향으로 한 칸 이동한다. 도착점이 맵 밖이면 기존 표현만 사용한다.
function PlayHitReaction(Direction, Distance, MinY, MaxY)
    State.Knockback = nil
    if Direction and Refs.PlayerRoot and MinY and MaxY then
        local Start = Refs.PlayerRoot:GetWorldLocation()
        local TargetY = Start.Y + Direction.Y * Distance
        if TargetY >= MinY and TargetY <= MaxY then
            State.Knockback = {
                Start = Start, Target = FVector.new(Start.X, TargetY, Start.Z), Elapsed = 0
            }
        end
    end
    if Refs.Mesh and State.Scale then
        State.SquashStartZ = Refs.Mesh:GetRelativeScale3D().Z
        State.SquashElapsed = 0
    end
end

-- 압축과 Knockback은 HitStop·슬로모션에 영향을 받지 않는 실제 시간을 사용한다.
function Update(DeltaSeconds)
    local Knockback = State.Knockback
    if Knockback then
        Knockback.Elapsed = math.min(Knockback.Elapsed + DeltaSeconds, Config.KnockbackDuration)
        local Alpha = Knockback.Elapsed / Config.KnockbackDuration
        local Progress = 1 - (1 - Alpha) * (1 - Alpha)
        local Start, Target = Knockback.Start, Knockback.Target
        Refs.PlayerRoot:SetWorldLocation(FVector.new(Start.X,
            Start.Y + (Target.Y - Start.Y) * Progress, Start.Z), false)
        if Alpha >= 1 then State.Knockback = nil end
    end

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
    State.Knockback = nil
    State.SquashElapsed = nil
    if Refs.Mesh and State.Scale then
        Refs.Mesh:SetRelativeScale3D(State.Scale)
        Refs.Mesh:SetRelativeLocation(State.Location)
        Refs.Mesh:SetRelativeRotation(State.Rotation)
    end
end
