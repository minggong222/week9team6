print("[GameManager] SCRIPT LOADED")

local GamePhase = {
    Ready = "Ready",
    Playing = "Playing",
    Dead = "Dead",
    Clear = "Clear"
}

_G.CrossyGameState = _G.CrossyGameState or {
    Phase = GamePhase.Ready,
    Score = 0,
    MaxRow = 0
}

local GameState = _G.CrossyGameState

local Player = nil
local StartPosition = nil
local ScoreText = nil
local Camera = nil

local CellSize = 1.0

local function StartGame()
    GameState.Phase = GamePhase.Playing
    GameState.Score = 0
    GameState.MaxRow = 0

    print("Game Started")
end

local function GameOver()
    if GameState.Phase ~= GamePhase.Playing then
        return
    end

    GameState.Phase = GamePhase.Dead

    if ScoreText then
        ScoreText:SetText("GAME OVER\nScore: " .. GameState.Score .. "\nPress R to Restart")
    end

    print("Game Over!")
end

local function Restart()
    if Player and StartPosition then
        Player:GetRootComponent():SetWorldLocation(StartPosition, false)
    end

    StartGame()

    if ScoreText then
        ScoreText:SetText("Score: 0")
    end
end

function BeginPlay(Self)
    Player = Actor:GetWorld():GetPlayerPawn()

    if Player then
        StartPosition = Player:GetActorLocation()
    end

    ScoreText = UTextRenderComponent.Cast(Actor:FindComponentByName("ScoreText"))
    Camera = Actor:GetWorld():GetPlayerCamera()

    if ScoreText then
        ScoreText:SetText("Score: 0")
        ScoreText:SetTextSize(0.25)
    else
        print("[GameManager] ScoreText not found")
    end

    StartGame()
end

function TickComponent(Self, DeltaSeconds)
    if ScoreText and Camera then
        local Rotation = Camera:GetWorldRotation()
        local Quaternion = Rotation:Quaternion()
        local Location = Camera:GetWorldLocation()

        local TextLocation = Location
            + Quaternion:GetForwardVector() * 3.0
            + Quaternion:GetUpVector() * 0.9

        ScoreText:SetWorldLocation(TextLocation, false)
        ScoreText:SetWorldRotation(Rotation, false)
    end
    
    if GameState.Phase == GamePhase.Playing and Input.IsKeyPressed(EKeyCode.K) then
        GameOver()
    end

    if GameState.Phase == GamePhase.Dead and Input.IsKeyPressed(EKeyCode.R) then
        Restart()
    end

    if GameState.Phase == GamePhase.Playing and Player and StartPosition then
    local Position = Player:GetActorLocation()
    local Row = math.max(0, math.floor((Position.x - StartPosition.x) / CellSize))

    if Input.IsKeyPressed(EKeyCode.P) then
        print("Start X: " .. StartPosition.x)
        print("Current X: " .. Position.x)
        print("Current Y: " .. Position.y)
        print("Row: " .. Row)
    end

    if Row > GameState.MaxRow then
        GameState.MaxRow = Row
        GameState.Score = Row

        if ScoreText then
            ScoreText:SetText("Score: " .. GameState.Score)
        end

        print("Score: " .. GameState.Score)
    end
end
end