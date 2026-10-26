
print("[GameManager] SCRIPT LOADED")

-- 게임 진행 상태
local GamePhase = {
    Ready = "Ready",
    Playing = "Playing",
    Dead = "Dead",
    Clear = "Clear"
}

-- 다른 Lua 스크립트와 공유하는 게임 상태
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
local GoalRow = 40  -- 실제 맵의 마지막 행에 맞게 수정

local function StartGame()
    GameState.Phase = GamePhase.Playing
    GameState.Score = 0
    GameState.MaxRow = 0
    GameState.GameOverRequested = false
    GameState.ClearRequested = false

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

local function GameClear()
    if GameState.Phase ~= GamePhase.Playing then
        return
    end

    GameState.Phase = GamePhase.Clear

    if ScoreText then
        ScoreText:SetText("GAME CLEAR\nScore: " .. GameState.Score .. "\nPress R to Restart")
    end

    print("Game Clear!")
end

local function Restart()
    -- 플레이어를 시작 위치로 복귀
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
    -- 점수 텍스트를 카메라 앞에 고정
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

    -- K키로 게임 오버 테스트
    if GameState.Phase == GamePhase.Playing and Input.IsKeyPressed(EKeyCode.K) then
        GameState.GameOverRequested = true
    end

    -- 외부 충돌 이벤트에서 전달한 사망 요청 처리
    if GameState.GameOverRequested then
        GameState.GameOverRequested = false
        GameOver()
    end

    -- 사망 또는 클리어 후 R키로 재시작
    if (GameState.Phase == GamePhase.Dead or GameState.Phase == GamePhase.Clear)
        and Input.IsKeyPressed(EKeyCode.R) then
        Restart()
    end

    if GameState.Phase == GamePhase.Playing and Player and StartPosition then
        local Position = Player:GetActorLocation()
        local Row = math.max(0, math.floor((Position.x - StartPosition.x) / CellSize))

        -- P키로 현재 위치와 진행 행 확인
        if Input.IsKeyPressed(EKeyCode.P) then
            print("Start X: " .. StartPosition.x)
            print("Current X: " .. Position.x)
            print("Current Y: " .. Position.y)
            print("Row: " .. Row)
        end

        -- 가장 멀리 전진한 행을 점수로 사용
        if Row > GameState.MaxRow then
            GameState.MaxRow = Row
            GameState.Score = Row

            if ScoreText then
                ScoreText:SetText("Score: " .. GameState.Score)
            end

            print("Score: " .. GameState.Score)
        end

        -- 마지막 행에 도달하면 클리어 요청
        if Row >= GoalRow then
            GameState.ClearRequested = true
        end
    end

    -- 맵 도착 판정 또는 외부 이벤트의 클리어 요청 처리
    if GameState.ClearRequested then
        GameState.ClearRequested = false
        GameClear()
    end
end
