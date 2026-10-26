
-- 컴포넌트가 초기화될 때 호출합니다.
function InitializeComponent(Self)

end
local MoveDistance = 1
local bWasWDown = false
local bWasSDown = false
local bWasADown = false
local bWasDDown = false
-- 컴포넌트가 시작될 때 호출합니다.
function BeginPlay(Self)

end

-- 컴포넌트가 Tick될 때 호출합니다.
-- @param DeltaSeconds 프레임 사이의 걸린 시간
function TickComponent(Self, DeltaSeconds)
    
    local bWDown = Input.IsKeyDown(EKeyCode.W)
    local bSDown = Input.IsKeyDown(EKeyCode.S)
    local bADown = Input.IsKeyDown(EKeyCode.A)
    local bDDown = Input.IsKeyDown(EKeyCode.D)
    
    local MoveDirection = FVector.new(0, 0, 0)
    
    if bWDown and not bWasWDown then
        MoveDirection = MoveDirection + FVector.new(1, 0, 0)
    end
    if bSDown and not bWasSDown then
        MoveDirection = MoveDirection + FVector.new(-1, 0, 0)
    end
    if bADown and not bWasADown then
        MoveDirection = MoveDirection + FVector.new(0, -1, 0)
    end
    if bDDown and not bWasDDown then
        MoveDirection = MoveDirection + FVector.new(0, 1, 0)
    end
    
    bWasWDown = bWDown
    bWasSDown = bSDown
    bWasADown = bADown
    bWasDDown = bDDown
    
    if MoveDirection:SizeSquared() > 0 then
        MoveDirection = MoveDirection:GetSafeNormal()
        local Root = Actor:GetRootComponent()
        if not Root then
            return
        end
        local Location = Root:GetWorldLocation()
        
        Root:SetWorldLocation(Location + MoveDirection * MoveDistance, false);
    end
end

-- 컴포넌트가 종료될 때 호출합니다.
function EndPlay(Self)

end

-- 컴포넌트가 제거될 때 호출합니다.
function Destroy(Self)

end