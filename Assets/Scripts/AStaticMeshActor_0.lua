
-- 컴포넌트가 초기화될 때 호출합니다.
function InitializeComponent(Self)

end

-- 컴포넌트가 시작될 때 호출합니다.
local Moving = false
local Direction = FVector.new(1, 0, 0)
local Speed = 1.0

function BeginPlay(Self)
    Moving = false
    StartCoroutine(function()
        while true do
            Wait(3)
            Moving = true
            Wait(1)
            Moving = false
        end
    end)
end

function TickComponent(Self, DeltaSeconds)
    if not Moving then return end

    local Root = Actor:GetRootComponent()
    if not Root then return end

    local Location = Root:GetWorldLocation()
    Root:SetWorldLocation(Location + Direction * Speed * DeltaSeconds, false)
end


-- 컴포넌트가 종료될 때 호출합니다.
function EndPlay(Self)

end

-- 컴포넌트가 제거될 때 호출합니다.
function Destroy(Self)

end