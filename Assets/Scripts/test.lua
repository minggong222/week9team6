local Moving = false
local Direction = FVector.new(1, 0, 0)
local Speed = 1.0

function BeginPlay(Self)
    Moving = false
    StartCoroutine(function()
        while true do
            Wait(1)
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
