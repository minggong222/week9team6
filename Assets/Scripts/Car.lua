local Direction = FVector.new(0.0, 1.0, 0.0)
local StartLocation = FVector.new(0.0, 0.0, 0.0)
local TotalDistance = 20.0
local Speed = 5.0
local Distance = 0.0
local bActive = false
local Car = nil
local LevelManager = nil

function SetCar(InCar, InLevelManager)
    -- Size는 추후 Collision Box 연결에 사용할 차량 설정으로 보관합니다.
    Car = InCar
    LevelManager = InLevelManager
end

function SetSpeed(InSpeed)
    Speed = math.max(0.0, InSpeed)
end

function SetStartLocation(InLocation)
    StartLocation = FVector.new(InLocation)
    Distance = 0.0
    Actor:GetRootComponent():SetWorldLocation(StartLocation, false)
end

function SetDirection(IsLeft)
    local RootComponent = Actor:GetRootComponent()

    if (IsLeft) then
        Direction = FVector.new(0.0, 1.0, 0.0)
        RootComponent:SetWorldRotation(FRotator.new(0.0, 270.0, 0.0), false)
    else
        Direction = FVector.new(0.0, -1.0, 0.0)
        RootComponent:SetWorldRotation(FRotator.new(0.0, 90.0, 0.0), false)
    end
end

function IsActive()
    return bActive
end

function Deactivate()
    bActive = false
    Actor:SetActorEnableCollision(false)
    Actor:GetRootComponent():SetVisibility(false, true)

    if LevelManager then
        LevelManager:Invoke("ReturnCar", Actor)
    end
end

function Activate(Location, InDirection, InSpeed, TravelDistance)
    Direction = InDirection:GetSafeNormal()
    TotalDistance = math.max(0.0, TravelDistance)
    SetSpeed(InSpeed)
    SetStartLocation(Location)

    bActive = true
    Actor:GetRootComponent():SetVisibility(true, true)
    Actor:SetActorEnableCollision(true)
end

local function CheckPlayerCollision(Delta)
    local GameState = _G.CrossyGameState
    -- 이동 중의 메시 외곽이 먼저 닿아도 죽이지 않고, 착지한 몸통 위치에서 판정한다.
    if GameState and GameState.PlayerIsJumping then return end
    local Player = Actor:GetWorld():GetPlayerPawn()
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

    -- 메시에는 Shape 충돌 이벤트가 없으므로 이동 구간과 플레이어 경계를 검사합니다.
    -- 선분과 확장된 AABB의 교차로 빠른 차량이 한 프레임에 통과하는 경우도 처리합니다.
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

    if GameState and GameState.GameOver then
        GameState.GameOver()
    end
end

function TickComponent(Self, DeltaSeconds)
    local GameState = _G.CrossyGameState
    if not bActive or not GameState
        or (GameState.Phase ~= "Playing" and GameState.Phase ~= "Dead") then
        return
    end

    local NextDistance = math.min(Distance + Speed * DeltaSeconds, TotalDistance)
    -- 게임 오버 후에는 이동만 유지하고 플레이어 충돌 판정은 생략합니다.
    if GameState.Phase == "Playing" then
        CheckPlayerCollision(Direction * (NextDistance - Distance))
    end

    Distance = NextDistance
    Actor:GetRootComponent():SetWorldLocation(StartLocation + Direction * Distance, false)

    if Distance >= TotalDistance then
        Deactivate()
    end
end
