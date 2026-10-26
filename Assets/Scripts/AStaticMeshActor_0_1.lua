
-- 컴포넌트가 초기화될 때 호출합니다.
function InitializeComponent(Self)

end
local MoveDistance = 1;
-- 컴포넌트가 시작될 때 호출합니다.
function BeginPlay(Self)

end

-- 컴포넌트가 Tick될 때 호출합니다.
-- @param DeltaSeconds 프레임 사이의 걸린 시간
function TickComponent(Self, DeltaSeconds)

    local Root = Actor:GetRootComponent()
    if not Root then
        return
    end
    local MoveDirection = FVector.new(0, 0, 0)
    
    if Input.IsKeyPressed(EKeyCode.W) then
        MoveDirection = MoveDirection + FVector.new(1, 0, 0)
    end
    if Input.IsKeyPressed(EKeyCode.S) then
        MoveDirection = MoveDirection + FVector.new(-1, 0, 0)
    end
    if Input.IsKeyPressed(EKeyCode.A) then
        MoveDirection = MoveDirection + FVector.new(0, -1, 0)
    end
    if Input.IsKeyPressed(EKeyCode.D) then
        MoveDirection = MoveDirection + FVector.new(0, 1, 0)
    end

    if MoveDirection:SizeSquared() > 0 then
        MoveDirection = MoveDirection:GetSafeNormal()
        
        local Location = Root:GetWorldLocation()
        local NewLocation = Location + MoveDirection * MoveDistance
        
        Root:SetWorldLocation(NewLocation, false);
    end
end

-- 컴포넌트가 종료될 때 호출합니다.
function EndPlay(Self)

end

-- 컴포넌트가 제거될 때 호출합니다.
function Destroy(Self)

end