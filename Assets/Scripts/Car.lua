-- 차량 이동 상태와 풀·매니저 참조는 이 스크립트에서만 보관
State = {}
Refs = {}
State.Direction = FVector.new(0.0, 1.0, 0.0)
State.StartLocation = FVector.new(0.0, 0.0, 0.0)
State.TotalDistance = 20.0
State.Speed = 5.0
State.Distance = 0.0
State.bActive = false

function SetCar(InCar, TrafficManager, LevelManager, Player, PlayerScript)
    Refs.Car, Refs.Traffic, Refs.Level = InCar, TrafficManager, LevelManager
    Refs.Player, Refs.PlayerScript = Player, PlayerScript
end

function SetSpeed(InSpeed)
    State.Speed = math.max(0.0, InSpeed)
end

function SetStartLocation(InLocation)
    State.StartLocation = FVector.new(InLocation)
    State.Distance = 0.0
    Actor:GetRootComponent():SetWorldLocation(State.StartLocation, false)
end

function SetDirection(IsLeft)
    local RootComponent = Actor:GetRootComponent()

    if (IsLeft) then
        State.Direction = FVector.new(0.0, 1.0, 0.0)
        RootComponent:SetWorldRotation(FRotator.new(0.0, 270.0, 0.0), false)
    else
        State.Direction = FVector.new(0.0, -1.0, 0.0)
        RootComponent:SetWorldRotation(FRotator.new(0.0, 90.0, 0.0), false)
    end
end

function IsActive()
    return State.bActive
end

function Deactivate()
    State.bActive = false
    Actor:SetActorEnableCollision(false)
    Actor:GetRootComponent():SetVisibility(false, true)

    if Refs.Traffic then
        Refs.Traffic:Invoke("ReturnCar", Actor)
    end
end

function Activate(Location, InDirection, InSpeed, TravelDistance)
    State.Direction = InDirection:GetSafeNormal()
    State.TotalDistance = math.max(0.0, TravelDistance)
    SetSpeed(InSpeed)
    SetStartLocation(Location)

    State.bActive = true
    Actor:GetRootComponent():SetVisibility(true, true)
    Actor:SetActorEnableCollision(true)
end

local function CheckPlayerCollision(Delta)
    -- 이동 중 메시 외곽 접촉은 제외하고 착지한 몸통 위치에서 사망 판정
    if Refs.PlayerScript and Refs.PlayerScript:Invoke("IsJumping") then return end
    local Player = Refs.Player
    if not Player then
        return
    end

    local HasCarBounds, CarBounds = Actor:TryGetActorBounds()
    local Hitbox = UBoxComponent.Cast(Player:FindComponentByName("PlayerTreeHitbox"))
    local HasPlayerBounds, PlayerBounds
    if Hitbox then
        HasPlayerBounds, PlayerBounds = true, Hitbox:CalcBounds()
    else
        HasPlayerBounds, PlayerBounds = Player:TryGetActorBounds()
    end
    if not HasCarBounds or not HasPlayerBounds then
        return
    end

    -- 메시에는 Shape 충돌 이벤트가 없으므로 이동 구간과 플레이어 경계 검사
    -- 선분과 확장된 AABB의 교차로 빠른 차량이 한 프레임에 통과하는 경우도 처리
    local EnterTime, ExitTime = 0.0, 1.0
    for _, Axis in ipairs({ "X", "Y", "Z" }) do
        local Min = PlayerBounds.Min[Axis] - CarBounds.Max[Axis]
        local Max = PlayerBounds.Max[Axis] - CarBounds.Min[Axis]
        local Movement = Delta[Axis]

        if math.abs(Movement) < 0.000001 then
            if Min > 0.0 or Max < 0.0 then
                return
            end
        else
            local First, Last = Min / Movement, Max / Movement
            EnterTime = math.max(EnterTime, math.min(First, Last))
            ExitTime = math.min(ExitTime, math.max(First, Last))
            if EnterTime > ExitTime then
                return
            end
        end
    end

    if Refs.Level:Invoke("IsGameManaged") then
        Refs.Level:Invoke("RequestGameOver", nil, State.Direction)
    end
end

function TickComponent(Self, DeltaSeconds)
    if not State.bActive or not Refs.Level then return end
    local Phase = Refs.Level:Invoke("GetPhase")
    if Phase ~= "Playing" and Phase ~= "Dead" and Phase ~= "Credits" then return end

    local NextDistance = math.min(State.Distance + State.Speed * DeltaSeconds, State.TotalDistance)
    -- 게임 오버·크레딧 화면에서는 이동만 유지하고 플레이어 충돌 판정 생략
    if Phase == "Playing" then
        CheckPlayerCollision(State.Direction * (NextDistance - State.Distance))
    end

    State.Distance = NextDistance
    Actor:GetRootComponent():SetWorldLocation(State.StartLocation + State.Direction * State.Distance, false)

    if State.Distance >= State.TotalDistance then
        Deactivate()
    end
end
