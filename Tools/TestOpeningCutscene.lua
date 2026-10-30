-- 실행: Source/ThirdParty/luajit/luajit.exe Tools/TestOpeningCutscene.lua
-- 실제 GameManager 스크립트로 첫 진입 컷신과 복귀 흐름을 검사합니다.
local function Setup(Available)
    local Calls, Pressed, Playing = {}, false, false
    local function Record(Name)
        return { Invoke = function(_, Method)
            local Key = Name .. "." .. Method
            Calls[Key] = (Calls[Key] or 0) + 1
            if Method == "PlayRestart" then return 0 end
        end }
    end
    local World = {
        GetRealDeltaSeconds = function() return 1 / 60 end,
        ResetTimeEffects = function() end,
        PlaySequence = function(_, Path)
            assert(Path == "Assets/Sequences/cutscene.json")
            Calls.PlaySequence = (Calls.PlaySequence or 0) + 1
            Playing = Available
            return Available
        end,
        IsSequencePlaying = function() return Playing end,
        StopSequence = function() Playing = false; Calls.StopSequence = (Calls.StopSequence or 0) + 1 end,
    }
    local Env = setmetatable({
        _G = {}, print = function() end,
        Actor = { GetWorld = function() return World end },
        EKeyCode = { R = 82 },
        Input = { IsKeyPressed = function(Key) return Key == 82 and Pressed end },
    }, { __index = _G })
    local Chunk = assert(loadfile("Assets/Scripts/GameManager.lua"))
    setfenv(Chunk, Env); Chunk()
    for _, Name in ipairs({"HitReaction", "Cinematics", "HUD", "Camera", "Selection", "PlayerScript"}) do
        Env.Refs[Name] = Record(Name)
    end
    Env.State.Game.Phase = "SelectingCharacter"
    return Env, Calls, World, function(Value) Pressed = Value end
end
local Game, Calls, World, Press = Setup(true)
Game.StartGame()
assert(Game.State.Game.Phase == "Intro" and Calls.PlaySequence == 1)
assert(Calls["HUD.ShowScore"] == 1 and not Calls["HUD.Hide"])
assert(Calls["PlayerScript.StopMovement"] == 1)
Game.TickComponent(nil, 0.1)
assert(Game.State.Game.Phase == "Intro")
assert(Calls["HUD.ShowScore"] == 2)
World:StopSequence()
Game.TickComponent(nil, 0.1)
assert(Game.State.Game.Phase == "Playing" and Calls.PlaySequence == 1)
Press(true); Game.TickComponent(nil, 0.1); Press(false)
Game.TickComponent(nil, 0.1)
assert(Game.State.Game.Phase == "Playing" and Calls.PlaySequence == 1)
Game.StartGame()
assert(Calls.PlaySequence == 1, "Character reselection replayed the cutscene")
Game.EndPlay(nil)
assert(Calls.StopSequence == 2)
local Again, AgainCalls = Setup(true)
Again.StartGame()
assert(Again.State.Game.Phase == "Intro" and AgainCalls.PlaySequence == 1)
local Cancel, CancelCalls, CancelWorld, CancelPress = Setup(true)
Cancel.StartGame(); CancelPress(true); Cancel.TickComponent(nil, 0.1); CancelPress(false)
assert(Cancel.State.Restarting and CancelCalls.StopSequence == 1)
Cancel.TickComponent(nil, 0.1)
assert(Cancel.State.Game.Phase == "Playing" and CancelCalls.PlaySequence == 1)
local Missing, MissingCalls = Setup(false)
Missing.StartGame()
assert(Missing.State.Game.Phase == "Playing" and MissingCalls.PlaySequence == 1)
print("PASS: first-entry Intro, finish, restart/reselection once-only, new session, R cancellation, missing asset fallback, EndPlay cleanup.")

-- 실제 Player 스크립트의 Intro 차단과 일반 플레이 입력 복원을 검사합니다.
local Phase, ForwardDown = "Intro", true
local PlayerEnv = setmetatable({
    Actor = { GetRootComponent = function() return {} end, GetComponents = function() return {} end },
    FVector = { new = function(X, Y, Z) return { X = X, Y = Y, Z = Z } end },
    EKeyCode = { W = "W", Up = "Up", S = "S", Down = "Down", A = "A", Left = "Left", D = "D", Right = "Right" },
    Input = { IsKeyDown = function(Key) return Key == "W" and ForwardDown end },
}, { __index = _G })
local PlayerChunk = assert(loadfile("Assets/Scripts/Player.lua"))
setfenv(PlayerChunk, PlayerEnv); PlayerChunk()
PlayerEnv.BindGame({ Invoke = function(_, Method) if Method == "GetPhase" then return Phase end end })
PlayerEnv.TickComponent(nil, 0.01)
assert(#PlayerEnv.State.MoveQueue == 0, "Intro accepted movement")
Phase, ForwardDown = "Playing", false
PlayerEnv.TickComponent(nil, 0.01)
ForwardDown = true
PlayerEnv.TickComponent(nil, 0.01)
assert(#PlayerEnv.State.MoveQueue == 1, "Gameplay movement remains disabled")
Phase = "Intro"
PlayerEnv.TickComponent(nil, 0.01)
assert(#PlayerEnv.State.MoveQueue == 0, "Intro retained queued movement")

-- 실제 LevelManager가 컷신 중 차량을 갱신하는지 검사합니다.
local TrafficUpdates = 0
local LevelEnv = setmetatable({
    _G = {},
    Actor = { GetWorld = function() return { GetRealDeltaSeconds = function() return 0.1 end } end },
    Input = { IsKeyPressed = function() return false end },
    EKeyCode = {},
}, { __index = _G })
local LevelChunk = assert(loadfile("Assets/Scripts/LevelManager.lua"))
setfenv(LevelChunk, LevelEnv); LevelChunk()
LevelEnv.State.Initialized, LevelEnv.State.Game, LevelEnv.State.Info = true, {}, {}
LevelEnv.Refs.Layout = { HasMapPool = false }
LevelEnv.Refs.Game = { Invoke = function() return Phase end }
LevelEnv.Refs.Map = { Invoke = function() end }
LevelEnv.Refs.Traffic = { Invoke = function(_, Method)
    if Method == "Update" then TrafficUpdates = TrafficUpdates + 1 end
    if Method == "GetCarCount" then return 0 end
end }
LevelEnv.TickComponent(nil, 0.1)
assert(TrafficUpdates == 1, "Intro paused traffic")
Phase = "SelectingCharacter"
LevelEnv.TickComponent(nil, 0.1)
assert(TrafficUpdates == 1, "Selection unexpectedly enabled traffic")
print("PASS: Intro shows HUD and moves traffic, blocks player input and clears its queue; Playing enables movement.")

-- TrafficManager 호출뿐 아니라 개별 차량 스크립트의 실제 위치 변화도 검사합니다.
local VectorMeta = {}
local function Vector(X, Y, Z)
    return setmetatable({ X = X or 0, Y = Y or 0, Z = Z or 0 }, VectorMeta)
end
VectorMeta.__add = function(A, B) return Vector(A.X + B.X, A.Y + B.Y, A.Z + B.Z) end
VectorMeta.__mul = function(A, B) return Vector(A.X * B, A.Y * B, A.Z * B) end
local CarLocation
local CarEnv = setmetatable({
    FVector = { new = Vector },
    Actor = { GetRootComponent = function()
        return { SetWorldLocation = function(_, Location) CarLocation = Location end }
    end },
}, { __index = _G })
local CarChunk = assert(loadfile("Assets/Scripts/Car.lua"))
setfenv(CarChunk, CarEnv); CarChunk()
CarEnv.State.bActive, CarEnv.State.Speed, CarEnv.State.TotalDistance = true, 5, 100
CarEnv.Refs.Level = { Invoke = function() return Phase end }
Phase = "Intro"
CarEnv.TickComponent(nil, 0.5)
assert(CarLocation and CarLocation.Y == 2.5, "Individual car remains stationary during Intro")
Phase = "SelectingCharacter"
CarEnv.TickComponent(nil, 0.5)
assert(CarLocation.Y == 2.5, "Individual car moves during selection")
Phase = "Intro"
CarEnv.TickComponent(nil, 0.5)
assert(CarLocation.Y == 5, "Individual car failed to resume Intro movement")
print("PASS: individual Car moves during Intro without querying player collision, and stops during selection.")
