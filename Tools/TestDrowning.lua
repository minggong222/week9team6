local function Vector(X, Y, Z) return { X = X, Y = Y, Z = Z } end
local Config = dofile('Assets/Scripts/ConfigManager.lua')
local PlayedHold
local World = { GetPlayerCameraManager = function() return nil end,
    PlayHitStop = function(_, _, Hold) PlayedHold = Hold end }
local Base = setmetatable({ _G = {}, Actor = { GetWorld = function() return World end },
    FVector = { new = Vector }, FColor = { new = function(...) return {...} end },
    Audio = { PlaySFX = function() end }, print = function() end }, { __index = _G })
local function Load(Path)
    local Env = setmetatable({}, { __index = Base })
    setfenv(assert(loadfile('Assets/Scripts/' .. Path)), Env)()
    return Env
end
local Mesh = { Location = Vector(1, 2, 3), Scale = Vector(1, 1, 1), Visible = true }
function Mesh:GetRelativeLocation() return self.Location end
function Mesh:SetRelativeLocation(V) self.Location = V end
function Mesh:GetRelativeScale3D() return Vector(self.Scale.X, self.Scale.Y, self.Scale.Z) end
function Mesh:SetRelativeScale3D(V) self.Scale = V end
function Mesh:GetRelativeRotation() return {} end
function Mesh:SetRelativeRotation() end
function Mesh:SetVisibility(V) self.Visible = V end
local Reaction = Load('Effects/PlayerHitReaction.lua')
Reaction.Initialize(Mesh)
local Cinema = Load('Cinematics/GameCinematics.lua')
local Game = Load('GameManager.lua')
Game.State.Game.Phase, Game.State.Game.Score = 'Playing', 0
Game.Refs.HUD = { Invoke = function() end }
Game.Refs.Cinematics = { Invoke = function(_, Name, ...) return Cinema[Name](...) end }
Game.Refs.HitReaction = { Invoke = function(_, Name, ...) return Reaction[Name](...) end }
Game.GameOver(nil, 'Drowning')
assert(Game.State.Game.Phase == 'Dead' and PlayedHold == Config.Cinematics.HitStop.Hold)
assert(Reaction.State.SquashElapsed == 0 and Reaction.State.Knockback == nil)
Reaction.Update(PlayedHold / 2)
assert(Mesh.Location.Z == 3 and Mesh.Visible and Mesh.Scale.Z < 1)
Reaction.Update(PlayedHold / 2)
assert(Mesh.Location.Z == 3 and Mesh.Visible)
Reaction.Update(Config.HitReaction.Drowning.Duration / 2)
assert(math.abs(Mesh.Location.Z - (3 - Config.HitReaction.Drowning.Depth / 2)) < 0.000001)
assert(Mesh.Location.X == 1 and Mesh.Location.Y == 2 and Mesh.Visible)
Reaction.Update(Config.HitReaction.Drowning.Duration)
assert(not Mesh.Visible and Reaction.State.Drowning == nil)
Reaction.ResetPlayerPose()
assert(Mesh.Visible and Mesh.Location.Z == 3 and Mesh.Scale.Z == 1)
Reaction.PlayDrowning(PlayedHold)
Reaction.Update(PlayedHold + Config.HitReaction.Drowning.Duration / 2)
Reaction.ResetPlayerPose()
Reaction.Update(10)
assert(Mesh.Visible and Mesh.Location.Z == 3 and Mesh.Scale.Z == 1)
print('PASS drowning dispatch, configured HitStop hold, real-time sinking, hide and retry cancellation')
