-- 저장소 루트에서 Source/ThirdParty/luajit/luajit.exe Tools/TestHighScore.lua 실행
local function Load(Path, Bindings)
    local Env = setmetatable(Bindings or {}, { __index = _G })
    setfenv(assert(loadfile('Assets/Scripts/' .. Path)), Env)()
    return Env
end
local Game = Load('GameManager.lua', { _G = {}, print = function() end })
Game.Refs.Cinematics = { Invoke = function() end }
Game.Refs.HUD = { Invoke = function() end }
Game.State.Game.Mode, Game.State.Game.Score = 'Day', 5
Game.UpdateHighScore()
assert(Game.GetHiScore('Day') == 5 and Game.GetHiScore('Night') == 0 and Game.State.Game.NewRecord)
Game.StartGame()
assert(Game.GetHiScore('Day') == 5 and not Game.State.Game.NewRecord)
Game.State.Game.Score = 5
Game.UpdateHighScore()
assert(not Game.State.Game.NewRecord, 'A tied score must not be a new record')
Game.State.Game.Score = 6
Game.UpdateHighScore()
Game.State.Game.Mode = 'Night'
Game.StartGame()
Game.State.Game.Score = 3
Game.UpdateHighScore()
assert(Game.GetHiScore('Night') == 3 and Game.GetHiScore('Day') == 6)
Game.State.Game.Mode = 'Day'
Game.StartGame()
assert(Game.GetHiScore() == 6 and not Game.State.Game.NewRecord)
local Fresh = Load('GameManager.lua', { _G = {}, print = function() end })
assert(Fresh.GetHiScore('Day') == 0 and Fresh.GetHiScore('Night') == 0)

local Meta = {}
local function Vector(X, Y, Z) return setmetatable({ X = X, Y = Y, Z = Z }, Meta) end
Meta.__add = function(A, B) return Vector(A.X + B.X, A.Y + B.Y, A.Z + B.Z) end
Meta.__mul = function(A, B) return Vector(A.X * B, A.Y * B, A.Z * B) end
local function Rotation(P, Y, R)
    return { Pitch = P, Yaw = Y, Roll = R, Quaternion = function()
        return { GetForwardVector = function() return Vector(1, 0, 0) end,
            GetRightVector = function() return Vector(0, 1, 0) end,
            GetUpVector = function() return Vector(0, 0, 1) end }
    end }
end
local Texts, World = {}, { GetRealDeltaSeconds = function() return 0.2 end }
local Actor = { GetWorld = function() return World end,
    FindComponentByName = function(_, Name) return Texts[Name] end }
function Actor:AddComponent(_, Name)
    local C = { Text = '' }
    function C:SetRelativeScale3D() end
    function C:SetCollisionEnabled() end
    function C:SetCastShadow() end
    function C:SetVisibility(V) self.Visible = V end
    function C:SetText(V) self.Text = V end
    function C:GetText() return self.Text end
    function C:SetTextSize(V) self.Size = V end
    function C:SetTextColor(V) self.Color = V end
    function C:SetWorldLocation(V) self.Location = V end
    function C:SetWorldRotation(V) self.Rotation = V end
    Texts[Name] = C
    return C
end
local Camera = { GetAspectRatio = function() return 1.6 end, GetIsOrthogonal = function() return true end,
    GetOrthoWidth = function() return 36 end, GetWorldRotation = function() return Rotation(0, 0, 0) end,
    GetWorldLocation = function() return Vector(0, 0, 0) end }
local Self = { SetComponentTickEnabled = function(S, V) S.Enabled = V end }
local HUD = Load('UI/GameHUD.lua', { Actor = Actor, FVector = { new = Vector }, FRotator = { new = Rotation },
    FColor = { new = function(...) return {...} end }, ECollisionEnabled = { NoCollision = 0 },
    UTextRenderComponent = { Cast = function(V) return V end, StaticClass = function() return 'Text' end } })
HUD.BeginPlay(Self)
HUD.Initialize(Camera)
HUD.ShowScore(6, 1, 6, true)
assert(Texts.HUDTitle.Text == 'SCORE' and Texts.HUDDetail.Text == 'HI-SCORE')
assert(Texts.ScoreText.Text == '006' and Texts.HiScoreText.Text == '006' and Texts.RecordText.Text == 'New Record!')
assert(Texts.HUDTitle.Location.Z > Texts.ScoreText.Location.Z and Texts.ScoreText.Location.Z > Texts.RecordText.Location.Z)
assert(Texts.HUDTitle.Location.Z == Texts.HUDDetail.Location.Z)
assert(Texts.ScoreText.Location.Z == Texts.HiScoreText.Location.Z)
assert(Texts.ScoreText.Location.Y < Texts.HiScoreText.Location.Y)
local FirstColor = Texts.ScoreText.Color
HUD.TickComponent(Self, 1)
assert(Self.Enabled and Texts.ScoreText.Color[2] ~= FirstColor[2], 'Record rainbow must keep animating after the score pulse')
HUD.ShowResult('Dead', 6, 1, 6, true)
assert(Texts.ScoreText.Location.Y < Texts.HiScoreText.Location.Y and Texts.RecordText.Visible)
assert(Texts.RecordText.Location.Z < Texts.ScoreText.Location.Z)
assert(math.abs(Texts.ScoreText.Location.Y + Texts.HiScoreText.Location.Y) < 0.000001)
HUD.ShowScore(0, 1, 6, false)
assert(Texts.ScoreText.Color[1] == 255 and Texts.ScoreText.Color[2] == 255 and not Texts.RecordText.Visible)
HUD.ShowResult('Dead', 2, 1, 6, false)
assert(Texts.ScoreText.Text == '002' and Texts.HiScoreText.Text == '006' and not Texts.RecordText.Visible)
HUD.ShowSelection('Beaver', 1, 2, 1, 'Day')
assert(not Texts.HiScoreText.Visible and not Texts.HiScoreTitle.Visible and not Texts.RecordText.Visible)
HUD.Hide()
assert(not Texts.ScoreText.Visible and not Self.Enabled)
print('PASS per-mode session records, ties/retry/reset, HUD layouts, persistent rainbow, and display cleanup')
