-- 게임 규칙과 시스템 간 호출 순서 관리
local GamePhase = { Selecting = "SelectingCharacter", Ready = "Ready", Playing = "Playing", Dead = "Dead", Clear = "Clear", Credits = "Credits" }
local Config = { CellSize = 3.0, GoalRow = 150 }

-- 전역 테이블은 기존 외부 호출과의 호환용. 게임 단계·점수 변경은 이 매니저 담당
State = { Game = _G.CrossyGameState or { Phase = GamePhase.Ready, Score = 0, MaxRow = 0 } }
Refs = {}
_G.CrossyGameState = State.Game
print("[GameManager] SCRIPT LOADED")

-- 기존 씬 연결 유지 및 분리한 스크립트의 동일 Actor 연결
local function AttachScript(Name, Path)
    local Script = ULuaComponent.Cast(Actor:FindComponentByName(Name))
        or ULuaComponent.Cast(Actor:AddComponent(ULuaComponent.StaticClass(), Name))
    assert(Script, "Could not create script component: " .. Name)

    local NeedsLoad = Script:GetScriptPath() ~= Path or not Script:GetLoaded()
    assert(Script:SetScriptPath(Path), "Could not find script: " .. Path)
    Script:InitializeLua()
    if NeedsLoad then assert(Script:LoadLua(), "Could not load script: " .. Path) end
    Script:SetComponentTickEnabled(false)
    return Script
end

-- HUD에 표시할 값만 전달. HUD의 다른 시스템 직접 조회 방지
local function UpdateHUD()
    local Game = State.Game
    if Game.Phase == GamePhase.Selecting then
        local Name, Index, Count = Refs.Selection:Invoke("GetSelection")
        Refs.HUD:Invoke("ShowSelection", Name, Index, Count, Game.HUDScale)
    elseif Game.Phase == GamePhase.Playing then
        Refs.HUD:Invoke("ShowScore", Game.Score, Game.HUDScale)
    elseif Game.Phase == GamePhase.Dead or Game.Phase == GamePhase.Clear then
        Refs.HUD:Invoke("ShowResult", Game.Phase, Game.Score, Game.HUDScale)
    elseif Game.Phase == GamePhase.Credits then
        Refs.HUD:Invoke("ShowCredits", Game.HUDScale, State.CreditsCamera)
    else
        Refs.HUD:Invoke("Hide")
    end
end

-- 연출과 HUD를 각각 갱신한다. 재시작의 Ready 처리도 FadeOut보다 먼저 끝낸다.
local function UpdateScreen()
    Refs.Cinematics:Invoke("Update", State.Game.Phase)
    UpdateHUD()
end

function BindLevel(LevelManager)
    Refs.Level = LevelManager
end

function GetPhase()
    return State.Game.Phase
end

-- Player가 소유한 이동 상태를 호환용 게임 정보에도 반영
function SetPlayerJumping(IsJumping)
    State.Game.PlayerIsJumping = IsJumping
end

function AcknowledgePlayerReset()
    State.Game.PlayerResetRequested = false
end

local function RequestPlayerReset(Requested)
    State.Game.PlayerResetRequested = Requested
    if Refs.PlayerScript then Refs.PlayerScript:Invoke("RequestReset", Requested) end
end

function StartGame()
    local Game = State.Game
    Game.Phase = GamePhase.Playing
    Game.Score, Game.MaxRow = 0, 0
    Game.GameOverRequested, Game.ClearRequested = false, false
    RequestPlayerReset(false)
    UpdateScreen()
    print("Game Started")
end

function GameOver()
    -- 같은 프레임의 여러 충돌도 한 번만 처리
    if State.Game.Phase ~= GamePhase.Playing then return end
    State.Game.Phase = GamePhase.Dead

    if Refs.PlayerScript then Refs.PlayerScript:Invoke("StopMovement") end
    Refs.HitReaction:Invoke("PlayHitReaction")
    Refs.Cinematics:Invoke("PlayDeath")
    UpdateScreen()
    print("Game Over!")
end

local function GameClear()
    if State.Game.Phase ~= GamePhase.Playing then return end
    State.Game.Phase = GamePhase.Clear
    UpdateScreen()
    print("Game Clear!")
end

local function ResetRun()
    Actor:GetWorld():ResetTimeEffects()
    State.Game.Phase = GamePhase.Ready
    if Refs.PlayerScript then Refs.PlayerScript:Invoke("StopMovement") end
    if Refs.Player and State.StartPosition then
        Refs.Player:GetRootComponent():SetWorldLocation(State.StartPosition, false)
    end

    Refs.HitReaction:Invoke("ResetPlayerPose")
    if Refs.Level then Refs.Level:Invoke("ResetLevel", true) end
    State.Game.Score, State.Game.MaxRow = 0, 0
    State.Game.GameOverRequested, State.Game.ClearRequested = false, false
    RequestPlayerReset(true)
end

local function Restart()
    if State.Restarting then return end
    local WasDead = State.Game.Phase == GamePhase.Dead
    State.Game.RestartCount = (State.Game.RestartCount or 0) + 1
    State.Restarting = true
    State.Game.Phase = GamePhase.Ready
    -- 사망 때 이미 중단했으므로 자세 복원은 암전 뒤 ResetRun에서만 수행한다.
    if Refs.PlayerScript and not WasDead then Refs.PlayerScript:Invoke("StopMovement") end
    UpdateScreen()
    State.RestartRemaining = Refs.Cinematics:Invoke("PlayRestart")
end

local function ReturnToCharacterSelection()
    ResetRun()
    State.Game.Phase = GamePhase.Selecting
    Refs.Selection:Invoke("ShowPreviews")
    State.Game.PreviewCharacter = Refs.Selection:Invoke("GetSelection")
    local Target = Refs.Selection:Invoke("GetSelectionCameraLocation")
    Refs.Camera:Invoke("ShowSelectionCamera", Target)
    UpdateScreen()
end

local function EnterSelectedCharacter()
    local Name, Index = Refs.Selection:Invoke("GetSelection")
    local Asset = Refs.Selection:Invoke("GetSelectedAsset")
    if not Refs.PlayerMesh or not Asset then
        print("[CharacterSelect] Cannot select " .. Name)
        return
    end

    if Refs.PlayerScript then Refs.PlayerScript:Invoke("StopMovement") end
    Refs.Selection:Invoke("ApplySelectedCharacter", Asset)
    Refs.HitReaction:Invoke("CapturePose")
    if Refs.PlayerScript then Refs.PlayerScript:Invoke("RefreshMeshBaseLocation") end

    State.Game.SelectedCharacter, State.Game.SelectedCharacterIndex = Name, Index
    Refs.Selection:Invoke("HidePreviews")
    Refs.Camera:Invoke("ApplyGameplayCamera")
    StartGame()
    print("[CharacterSelect] Confirmed " .. Name .. " -> Playing")
end

function OnPlayerOverlap(OtherComponent)
    local IsJumping = Refs.PlayerScript and Refs.PlayerScript:Invoke("IsJumping")
    if State.Game.Phase ~= GamePhase.Playing or IsJumping or not OtherComponent then return end
    if OtherComponent:GetName() == "CarHitbox" and not State.Game.GameOverRequested then
        State.Game.GameOverRequested = true
    end
end

function BeginPlay(Self)
    Refs.HUD = AttachScript("CrossyHUD", "Scripts/UI/GameHUD.lua")
    Refs.Selection = AttachScript("CrossyCharacterSelection", "Scripts/Character/CharacterSelection.lua")
    Refs.Camera = AttachScript("CrossyGameCamera", "Scripts/Camera/GameCamera.lua")
    Refs.HitReaction = AttachScript("CrossyPlayerHitReaction", "Scripts/Effects/PlayerHitReaction.lua")
    Refs.Cinematics = AttachScript("CrossyGameCinematics", "Scripts/Cinematics/GameCinematics.lua")
    Refs.Player = Actor:GetWorld():GetPlayerPawn()

    if Refs.Player then
        State.StartPosition = Refs.Player:GetActorLocation()
        Refs.PlayerMesh = UStaticMeshComponent.Cast(Refs.Player:FindComponentByName("StaticMeshComponent"))
        if not Refs.PlayerMesh then print("[GameManager] Player StaticMeshComponent not found") end
        for _, Component in ipairs(Refs.Player:GetComponents()) do
            local Script = ULuaComponent.Cast(Component)
            if Script and Script:GetScriptPath() == "Scripts/Player.lua" then Refs.PlayerScript = Script end
        end
        if Refs.PlayerScript then Refs.PlayerScript:Invoke("BindGame", Self) end
        Refs.Player:SetActorEnableCollision(true)

        Refs.PlayerHitbox = UPrimitiveComponent.Cast(Refs.Player:FindComponentByName("PlayerHitbox"))
        if Refs.PlayerHitbox then
            Refs.PlayerHitbox:SetCollisionResponseToAllChannels(ECollisionResponse.Overlap)
            State.BeginOverlapHandle = Refs.PlayerHitbox.OnComponentBeginOverlap:Add(function(OtherComponent)
                Self:Invoke("OnPlayerOverlap", OtherComponent)
            end)
            print("[GameManager] Collision ready")
        else
            print("[GameManager] PlayerHitbox not found")
        end
    end

    local Camera = Actor:GetWorld():GetPlayerCamera()
    Refs.HUD:Invoke("Initialize", Camera)
    Refs.Camera:Invoke("Initialize", Camera)
    Refs.Selection:Invoke("Initialize", Refs.Player, Refs.PlayerMesh)
    Refs.HitReaction:Invoke("Initialize", Refs.PlayerMesh)
    Refs.Cinematics:Invoke("PlayStart")

    -- 두 매니저의 BeginPlay 순서와 무관한 연결 및 기존 공개 인터페이스 유지
    State.Game.Manager = Self
    State.Game.GameOver = function() Self:Invoke("GameOver") end
    State.Game.ManagedByGameManager, State.Game.EnableTraffic = true, true
    local Level = _G.CrossyLevel
    if Level and Level.Manager then
        BindLevel(Level.Manager)
        Level.Manager:Invoke("BindGame", Self)
        Level.Manager:Invoke("EnableTraffic")
    end

    State.Game.Phase, State.Game.Score, State.Game.MaxRow = GamePhase.Ready, 0, 0
    State.Game.GameOverRequested, State.Game.ClearRequested = false, false
    Refs.Selection:Invoke("CreatePreviews")
    EnterSelectedCharacter()
end

function TickComponent(Self, DeltaSeconds)
    local Game = State.Game
    Refs.HitReaction:Invoke("Update", DeltaSeconds)
    -- 암전 중에는 재입력을 막고, 초기화는 화면이 가려진 뒤 실행한다.
    if State.Restarting then
        State.RestartRemaining = State.RestartRemaining - DeltaSeconds
        if State.RestartRemaining <= 0 then
            ResetRun()
            State.Restarting = false
            StartGame()
            RequestPlayerReset(true)
        end
        return
    end

    if Game.Phase == GamePhase.Credits then
        if Input.IsKeyPressed(EKeyCode.C) or Input.IsKeyPressed(EKeyCode.Backspace) then
            Refs.Camera:Invoke("StopCreditsCamera")
            State.CreditsCamera = nil
            Game.Phase = State.CreditsReturnPhase
            State.CreditsReturnPhase = nil
            UpdateScreen()
        else
            Refs.Camera:Invoke("UpdateCreditsCamera", DeltaSeconds)
            UpdateHUD()
        end
        return
    end

    --------------------------------------------------------------------------------------
    -- 임시 F3로 정상 속도와 슬로모션 전환
    local bF3Down = Input.IsKeyPressed(EKeyCode.F3)

    if bF3Down and not State.TimeDilationF3WasDown then
        State.TimeDilationF3WasDown = true

        local World = Actor:GetWorld()
        local Current = World:GetGlobalTimeDilation()

        local Target = Current < 0.99 and 1.0 or 0.25

        if World:SetGlobalTimeDilation(Target) then
            print("[TimeDilationTest] " .. tostring(Target))
        end
    end

    if not bF3Down then
        State.TimeDilationF3WasDown = false
    end
    -- 임시 F4: Lua 카메라 시퀀스 테스트
    local bF4Down = Input.IsKeyPressed(EKeyCode.F4)

    if bF4Down and not State.CineCameraF4WasDown then
        State.CineCameraF4WasDown = true

        if Refs.Cinematics then
            Refs.Cinematics:Invoke("PlayCameraSequence", 3.0)
        end
    end

    if not bF4Down then
        State.CineCameraF4WasDown = false
    end
    -- 임시 CineCameraActor 테스트: F5
    local bF5Down = Input.IsKeyPressed(EKeyCode.F5)

    -- 키를 누르고 있는 동안 매 프레임 토글되는 것을 방지한다.
    if bF5Down and not State.CineCameraF5WasDown then
        State.CineCameraF5WasDown = true

        local World = Actor:GetWorld()

        if not State.CineCameraTestActor then
            -- CineCameraActor 생성
            local CineClass = UClass.Find("ACineCameraActor")

            if not CineClass then
                print("[CineCameraTest] ACineCameraActor class not found")
            else
                local Spawned = World:SpawnActor(
                    CineClass,
                    "CineCameraActor_Test"
                )

                if Spawned then
                    State.CineCameraTestActor =
                        ACineCameraActor.Cast(Spawned)

                    State.CineCameraFocalLength = 35.0
                else
                    print("[CineCameraTest] SpawnActor failed")
                end
            end
        else
            -- 35mm와 85mm를 번갈아 적용
            if State.CineCameraFocalLength == 35.0 then
                State.CineCameraFocalLength = 85.0
            else
                State.CineCameraFocalLength = 35.0
            end
        end

        local Cine = State.CineCameraTestActor

        if Cine then
            Cine:SetSensorWidth(36.0)
            Cine:SetFocalLength(State.CineCameraFocalLength)

            local Camera = Cine:GetCameraComponent()

            -- CineCameraActor의 카메라를 현재 뷰 타깃으로 설정
            World:SetMainCameraComponent(Camera, 0.0)

            print(
                "[CineCameraTest] FocalLength = "
                .. Cine:GetFocalLength()
                .. ", FOV = "
                .. Camera:GetFieldOfView()
            )
        end
    end

    -- 키를 뗐을 때 다음 입력을 받을 수 있도록 초기화
    if not bF5Down then
        State.CineCameraF5WasDown = false
    end
    -- 임시 Camera Transition 테스트: F6
    if Input.IsKeyPressed(EKeyCode.F6) then
        local World = Actor:GetWorld()
        local PlayerCamera = World:GetPlayerCamera()
        local MainCamera = World:GetMainCameraComponent()

        if not PlayerCamera or not MainCamera then
            print("[CameraTransitionTest] Camera not found")
            return
        end

        local TargetCamera
        local TargetName

        if State.CameraTransitionTestToMain then
            TargetCamera = PlayerCamera
            TargetName = "PlayerCamera"
        else
            TargetCamera = MainCamera
            TargetName = "MainCamera"
        end

        if World:SetMainCameraComponent(TargetCamera, 1.5) then
            State.CameraTransitionTestToMain =
                not State.CameraTransitionTestToMain

            print("[CameraTransitionTest] Transition to " .. TargetName)
        else
            print("[CameraTransitionTest] Failed to change ViewTarget")
        end

        return
    end
        -- 임시 Camera Modifier 테스트: F7
    if Input.IsKeyPressed(EKeyCode.F7) then
        local World = Actor:GetWorld()

        local CameraManager = World:GetPlayerCameraManager()

        if CameraManager and CameraManager:StartCameraShake(1.5, 2.0, 3.0, 16.0) then
            print("[CameraModifierTest] Camera shake started")
        else
            print("[CameraModifierTest] Failed to start camera shake")
        end

        return
    end
    -- 임시 HitStop 테스트: 즉시 0.5초 정지한 뒤 5.0초 동안 원래 속도로 복귀한다.
    if Input.IsKeyPressed(EKeyCode.F9) then
        if Actor:GetWorld():PlayHitStop(0.0, 0.5, 5.0) then
            print("[HitStopTest] HitStop started")
        end
    end

    --------------------------------------------------------------------------------------
    local CanSelectCharacter = Game.Phase == GamePhase.Dead
        or (Game.Phase == GamePhase.Playing and (Game.Score or 0) == 0)
    if CanSelectCharacter and Input.IsKeyPressed(EKeyCode.C) then
        State.CreditsReturnPhase = Game.Phase
        if Game.Phase == GamePhase.Playing and Refs.PlayerScript then Refs.PlayerScript:Invoke("StopMovement") end
        State.CreditsCamera = Refs.Camera:Invoke("StartCreditsCamera")
        Game.Phase = GamePhase.Credits
        UpdateScreen()
        return
    end
    if CanSelectCharacter and Input.IsKeyPressed(EKeyCode.Backspace) then
        ReturnToCharacterSelection()
        return
    end

    if Game.Phase == GamePhase.Selecting then
        local Changed, Confirmed = Refs.Selection:Invoke("HandleInput")
        if Changed then
            Game.PreviewCharacter = Refs.Selection:Invoke("GetSelection")
            UpdateScreen()
        end
        if Confirmed then
            EnterSelectedCharacter()
            return
        end

        local Target = Refs.Selection:Invoke("GetSelectionCameraLocation")
        Refs.Camera:Invoke("UpdateSelectionCamera", DeltaSeconds, Target)
        UpdateScreen()
        return
    end

    -- K키 사망 요청과 외부 충돌 요청을 같은 경로로 처리
    if Game.Phase == GamePhase.Playing and Input.IsKeyPressed(EKeyCode.K) then Game.GameOverRequested = true end
    if Game.GameOverRequested then
        Game.GameOverRequested = false
        GameOver()
    end
    if Input.IsKeyPressed(EKeyCode.R) then
        Restart()
        return
    end

    if Game.Phase == GamePhase.Playing and Refs.Player and State.StartPosition then
        local Position = Refs.Player:GetActorLocation()
        local HighestRow, IsEndless, Step = 0, false, Config.CellSize
        if Refs.Level then HighestRow, IsEndless, Step = Refs.Level:Invoke("GetProgress") end
        local Row = math.max(0, math.floor((Position.X - State.StartPosition.X) / Step + 0.0001))

        if Input.IsKeyPressed(EKeyCode.P) then
            print("Start X: " .. State.StartPosition.X)
            print("Current X: " .. Position.X)
            print("Current Y: " .. Position.Y)
            print("Row: " .. Row)
        end

        local NewScore = math.max(Game.Score or 0, Game.MaxRow or 0, Row, IsEndless and HighestRow or 0)
        if NewScore > (Game.Score or 0) then print("Score: " .. NewScore) end
        Game.MaxRow, Game.Score = NewScore, NewScore
        if not IsEndless and Row >= Config.GoalRow then Game.ClearRequested = true end
    end

    if Game.ClearRequested then
        Game.ClearRequested = false
        GameClear()
    end
    UpdateScreen()
end

function EndPlay(Self)
    Refs.Camera:Invoke("StopCreditsCamera")
    Refs.Cinematics:Invoke("StopCinematics")
    if Refs.PlayerHitbox and State.BeginOverlapHandle then
        Refs.PlayerHitbox.OnComponentBeginOverlap:Remove(State.BeginOverlapHandle)
    end
    State.BeginOverlapHandle = nil
    Refs.PlayerHitbox = nil
    Refs.Selection:Invoke("DestroyPreviews")
    if _G.CrossyGameState == State.Game then _G.CrossyGameState = nil end
end
