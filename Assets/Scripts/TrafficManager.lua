
-- 컴포넌트가 초기화될 때 호출합니다.
function InitializeComponent(Self)

end

-- 컴포넌트가 시작될 때 호출합니다.
function BeginPlay(Self)

end

local Direction = FVector.new(1.0, 0.0, 0.0)
local StartLocation = FVector.new(0.0, 0.0, 0.0)
local TotalDistance = 10.0
local Speed = 5.0

Distance = 0.0

-- 컴포넌트가 Tick될 때 호출합니다.
-- @param DeltaSeconds 프레임 사이의 걸린 시간
function TickComponent(Self, DeltaSeconds)
	Distance += Speed * DeltaSeconds

	if (Distance > TotalDistance) then
		Distance -= TotalDistance
	end

	local RootComponent = Actor:GetRootComponent()
	local Location = RootComponent:GetRelativeLocation()

	Location = Direction * Distance

	RootComponent:SetRelativeLocation(Location)
end

-- 컴포넌트가 종료될 때 호출합니다.
function EndPlay(Self)

end

-- 컴포넌트가 제거될 때 호출합니다.
function Destroy(Self)

end