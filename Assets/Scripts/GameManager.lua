print("[GameManager] SCRIPT LOADED")

-- 게임 진행 상태
local GamePhase = {
    Selecting = "SelectingCharacter",
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
local PlayerMesh = nil
local PlayerMeshStartScale = nil
local PlayerMeshStartLocation, PlayerMeshStartRotation, PlayerScript = nil, nil, nil
local ScoreText = nil
local HUDTitle, HUDDetail, HUDHint = nil, nil, nil
local Camera = nil

-- 화면 높이에 대한 글자 크기. 실행 중 전체 배율은 CrossyGameState.HUDScale로 조절한다.
local HUDStyle = { Scale = 1.0, Score = 0.105, Title = 0.032, Name = 0.052, Hint = 0.023 }
local GameplayCamera = { Pitch = 40.0, Yaw = -35.0, Width = 36.0, Distance = 32.0, LookAhead = 5.0 }

local PlayerHitbox = nil
local BeginOverlapHandle = nil

-- origin/main의 캐릭터 선택 목록. 프리뷰와 실제 플레이 메시를 같은 비율로 맞춥니다.
local CharacterOptions = {
    { Name = "Beaver", Path = "Models/OBJ format/animal-beaver.obj" },
    { Name = "Bee", Path = "Models/OBJ format/animal-bee.obj" },
    { Name = "Bunny", Path = "Models/OBJ format/animal-bunny.obj" },
    { Name = "Cat", Path = "Models/OBJ format/animal-cat.obj" },
    { Name = "Caterpillar", Path = "Models/OBJ format/animal-caterpillar.obj" },
    { Name = "Chick", Path = "Models/OBJ format/animal-chick.obj" },
    { Name = "Cow", Path = "Models/OBJ format/animal-cow.obj" },
    { Name = "Crab", Path = "Models/OBJ format/animal-crab.obj" },
    { Name = "Deer", Path = "Models/OBJ format/animal-deer.obj" },
    { Name = "Dog", Path = "Models/OBJ format/animal-dog.obj" },
    { Name = "Elephant", Path = "Models/OBJ format/animal-elephant.obj" },
    { Name = "Fish", Path = "Models/OBJ format/animal-fish.obj" },
    { Name = "Fox", Path = "Models/OBJ format/animal-fox.obj" },
    { Name = "Giraffe", Path = "Models/OBJ format/animal-giraffe.obj" },
    { Name = "Hog", Path = "Models/OBJ format/animal-hog.obj" },
    { Name = "Koala", Path = "Models/OBJ format/animal-koala.obj" },
    { Name = "Lion", Path = "Models/OBJ format/animal-lion.obj" },
    { Name = "Monkey", Path = "Models/OBJ format/animal-monkey.obj" },
    { Name = "Panda", Path = "Models/OBJ format/animal-panda.obj" },
    { Name = "Parrot", Path = "Models/OBJ format/animal-parrot.obj" },
    { Name = "Penguin", Path = "Models/OBJ format/animal-penguin.obj" },
    { Name = "Pig", Path = "Models/OBJ format/animal-pig.obj" },
    { Name = "Polar", Path = "Models/OBJ format/animal-polar.obj" },
    { Name = "Tiger", Path = "Models/OBJ format/animal-tiger.obj" },
    { Name = "Hitori", Path = "Models/OBJ format/animal-hitori.obj" },
}
local SelectedCharacterIndex = 1
local CharacterPreviewActors = {}
local CharacterRowX, CharacterSpacing, CharacterSize = -200.0, 5.0, 2.0
local CameraMoveSpeed = 8.0
local GameplayCameraLocation, GameplayCameraRotation = nil, nil

local function FitCharacterMesh(Mesh, Owner)
    Mesh:SetRelativeLocation(FVector.new(0, 0, 0))
    Mesh:SetRelativeScale3D(FVector.new(1, 1, 1))
    local Bounds, Position = Mesh:CalcBounds(), Owner:GetActorLocation()
    local Size = math.max(Bounds.Max.X - Bounds.Min.X, Bounds.Max.Y - Bounds.Min.Y)
    assert(Size > 0, "Character has no horizontal bounds")
    local Scale = CharacterSize / Size
    Mesh:SetRelativeScale3D(FVector.new(Scale, Scale, Scale))
    Mesh:SetRelativeLocation(FVector.new(
        -(Bounds.Min.X + Bounds.Max.X - 2 * Position.X) * Scale / 2,
        -(Bounds.Min.Y + Bounds.Max.Y - 2 * Position.Y) * Scale / 2,
        -(Bounds.Min.Z - Position.Z) * Scale))
    return Mesh:GetRelativeScale3D()
end

local function GetCharacterRowY(Index)
    return (Index - (#CharacterOptions + 1) / 2) * CharacterSpacing
end

local function GetSelectionCameraLocation()
    return FVector.new(CharacterRowX - 8, GetCharacterRowY(SelectedCharacterIndex), 8.0)
end

local function CreateHUDText(Name)
    local Text = UTextRenderComponent.Cast(Actor:FindComponentByName(Name))
        or UTextRenderComponent.Cast(Actor:AddComponent(UTextRenderComponent.StaticClass(), Name))
    if Text then
        Text:SetRelativeScale3D(FVector.new(1, 1, 1))
        Text:SetCollisionEnabled(ECollisionEnabled.NoCollision)
        Text:SetCastShadow(false)
        Text:SetVisibility(false)
    end
    return Text
end

-- 좌표는 화면 비율(왼쪽/아래 0, 오른쪽/위 1), 크기는 화면 높이 비율이다.
-- 직교/원근 투영과 창 종횡비가 바뀌어도 같은 화면 비율을 유지한다.
local function UpdateHUD()
    if not ScoreText or not Camera then return end
    local Distance = 2.0
    local Aspect = math.max(Camera:GetAspectRatio(), 0.1)
    local Height = Camera:GetIsOrthogonal() and Camera:GetOrthoWidth() / Aspect
        or 2 * Distance * math.tan(math.rad(Camera:GetFieldOfView()) / 2)
    local Width = Height * Aspect
    local Rotation = Camera:GetWorldRotation()
    local Q = Rotation:Quaternion()
    local Center = Camera:GetWorldLocation() + Q:GetForwardVector() * Distance
    local Scale = math.max(0.5, math.min(1.5, GameState.HUDScale or HUDStyle.Scale))

    local function Place(Component, Text, Size, X, Y)
        if not Component then return end
        if Component:GetText() ~= Text then Component:SetText(Text) end
        Component:SetTextSize(Height * Size * Scale)
        Component:SetWorldLocation(Center + Q:GetRightVector() * ((X - 0.5) * Width)
            + Q:GetUpVector() * ((Y - 0.5) * Height), false)
        Component:SetWorldRotation(Rotation, false)
        Component:SetVisibility(Text ~= "")
    end

    if GameState.Phase == GamePhase.Selecting then
        local Option = CharacterOptions[SelectedCharacterIndex]
        Place(HUDTitle, "CHOOSE YOUR CHARACTER", HUDStyle.Title, 0.5, 0.88)
        Place(ScoreText, "<  " .. Option.Name:upper() .. "  >", HUDStyle.Name, 0.5, 0.25)
        Place(HUDDetail, string.format("%02d / %02d", SelectedCharacterIndex, #CharacterOptions), HUDStyle.Hint, 0.5, 0.18)
        Place(HUDHint, "LEFT / RIGHT : SELECT     ENTER : PLAY", HUDStyle.Hint, 0.5, 0.09)
    elseif GameState.Phase == GamePhase.Playing then
        Place(HUDTitle, "SCORE", HUDStyle.Hint, 0.13, 0.92)
        Place(ScoreText, string.format("%03d", GameState.Score or 0), HUDStyle.Score, 0.13, 0.81)
        Place(HUDDetail, "", HUDStyle.Hint, 0.5, 0.5)
        Place(HUDHint, "BACKSPACE : CHARACTERS", HUDStyle.Hint, 0.5, 0.06)
    elseif GameState.Phase == GamePhase.Dead or GameState.Phase == GamePhase.Clear then
        Place(HUDTitle, GameState.Phase == GamePhase.Dead and "GAME OVER" or "GAME CLEAR", 0.065, 0.5, 0.72)
        Place(ScoreText, string.format("%03d", GameState.Score or 0), HUDStyle.Score, 0.5, 0.55)
        Place(HUDDetail, "FINAL SCORE", HUDStyle.Hint, 0.5, 0.48)
        Place(HUDHint, "R : RETRY\nBACKSPACE : CHARACTERS", HUDStyle.Hint, 0.5, 0.34)
    else
        ScoreText:SetVisibility(false)
        if HUDTitle then HUDTitle:SetVisibility(false) end
        if HUDDetail then HUDDetail:SetVisibility(false) end
        if HUDHint then HUDHint:SetVisibility(false) end
    end
end

local function ApplyGameplayCamera()
    if not Camera then return end
    local Rotation = FRotator.new(GameplayCamera.Pitch, GameplayCamera.Yaw, 0)
    Camera:SetRelativeRotation(Rotation)
    Camera:SetRelativeLocation(FVector.new(GameplayCamera.LookAhead, 0, 0)
        - Rotation:Quaternion():GetForwardVector() * GameplayCamera.Distance)
    Camera:SetIsOrthogonal(true)
    Camera:SetOrthoWidth(GameplayCamera.Width)
end

local function ApplyCharacterSelection()
    for Index, Entry in pairs(CharacterPreviewActors) do
        Entry.Mesh:SetRelativeScale3D(Entry.BaseScale * (Index == SelectedCharacterIndex and 1.2 or 1.0))
    end
    GameState.PreviewCharacter = CharacterOptions[SelectedCharacterIndex].Name
    if GameState.Phase == GamePhase.Selecting then
        UpdateHUD()
    end
end

local function CreateCharacterPreviews()
    local World = Actor:GetWorld()
    for Index, Option in ipairs(CharacterOptions) do
        local Pose = FTransform.new()
        Pose.Location = FVector.new(CharacterRowX, GetCharacterRowY(Index), 0)
        local Preview = World:SpawnActor(UClass.Find("AStaticMeshActor"), "CharacterPreview_" .. Index, Pose)
        assert(Preview, "Could not create character preview")
        Preview:SetActorEnableCollision(false)
        local Mesh = UStaticMeshComponent.Cast(Preview:FindComponentByName("UStaticMeshComponent"))
        local Asset = UAssetManager:GetAssetByPath(Option.Path)
        if Mesh and Asset then
            Mesh:SetStaticMesh(Asset)
            CharacterPreviewActors[Index] = { Actor = Preview, Mesh = Mesh, BaseScale = FitCharacterMesh(Mesh, Preview) }
        else
            Preview:Destroy()
            print("[CharacterSelect] Missing preview: " .. Option.Path)
        end
    end
    ApplyCharacterSelection()
end

local CellSize = 3.0
local GoalRow = 150  -- 실제 맵의 마지막 행에 맞게 수정

local function StartGame()
    GameState.Phase = GamePhase.Playing
    GameState.Score = 0
    GameState.MaxRow = 0
    GameState.PlayerResetRequested = false
    GameState.GameOverRequested = false
    GameState.ClearRequested = false

    UpdateHUD()
    print("Game Started")
end

local function EnterSelectedCharacter()
    local Option = CharacterOptions[SelectedCharacterIndex]
    -- 강조 표시한 프리뷰와 같은 메시를 확정해 선택 화면/실제 캐릭터를 일치시킵니다.
    local Preview = CharacterPreviewActors[SelectedCharacterIndex]
    local Asset = Preview and Preview.Mesh:GetStaticMesh() or UAssetManager:GetAssetByPath(Option.Path)
    if not PlayerMesh or not Asset then print("[CharacterSelect] Cannot select " .. Option.Name); return end
    if PlayerScript then PlayerScript:Invoke("StopMovement") end
    PlayerMesh:SetStaticMesh(Asset)
    FitCharacterMesh(PlayerMesh, Player)
    PlayerMeshStartScale, PlayerMeshStartLocation = PlayerMesh:GetRelativeScale3D(), PlayerMesh:GetRelativeLocation()
    PlayerMeshStartRotation = PlayerMesh:GetRelativeRotation()
    if PlayerScript then PlayerScript:Invoke("RefreshMeshBaseLocation") end
    GameState.SelectedCharacter = Option.Name
    GameState.SelectedCharacterIndex = SelectedCharacterIndex
    for _, Entry in pairs(CharacterPreviewActors) do Entry.Actor:GetRootComponent():SetVisibility(false, true) end
    ApplyGameplayCamera()
    StartGame()
    print("[CharacterSelect] Confirmed " .. Option.Name .. " -> Playing")
end

local function GameOver()
    if GameState.Phase ~= GamePhase.Playing then
        return
    end

    GameState.Phase = GamePhase.Dead

    if PlayerScript then PlayerScript:Invoke("StopMovement") end

    if PlayerMesh and PlayerMeshStartScale then
        local Scale = PlayerMesh:GetRelativeScale3D()

        Scale.Z = PlayerMeshStartScale.Z * 0.25

        PlayerMesh:SetRelativeScale3D(Scale)
    end

    Audio.PlaySFX("Audio/Collision.wav")

    UpdateHUD()
    print("Game Over!")
end

-- 장애물에서도 UI를 포함한 동일한 게임 오버 처리를 사용합니다.
GameState.GameOver = GameOver

local function GameClear()
    if GameState.Phase ~= GamePhase.Playing then
        return
    end

    GameState.Phase = GamePhase.Clear

    UpdateHUD()
    print("Game Clear!")
end

local function ResetRun()
    -- 플레이어 위치와 크기 복원
    GameState.Phase = GamePhase.Ready
    if PlayerScript then PlayerScript:Invoke("StopMovement") end
    -- 플레이어를 시작 위치로 복귀
    if Player and StartPosition then
        Player:GetRootComponent():SetWorldLocation(StartPosition, false)
    end

    if PlayerMesh and PlayerMeshStartScale then
        PlayerMesh:SetRelativeScale3D(PlayerMeshStartScale)
        PlayerMesh:SetRelativeLocation(PlayerMeshStartLocation)
        PlayerMesh:SetRelativeRotation(PlayerMeshStartRotation)
    end
    -- 무한 맵의 차량/기차/회수 행도 같은 시작 상태로 되돌립니다.
    local Level = _G.CrossyLevel
    if Level then Level.Reset(true) end
    GameState.Score, GameState.MaxRow = 0, 0
    GameState.GameOverRequested, GameState.ClearRequested = false, false
    GameState.PlayerResetRequested = true
end

local function Restart()
    GameState.RestartCount = (GameState.RestartCount or 0) + 1
    ResetRun()
    StartGame()
    GameState.PlayerResetRequested = true
end

local function ReturnToCharacterSelection()
    ResetRun()
    GameState.Phase = GamePhase.Selecting
    for _, Entry in pairs(CharacterPreviewActors) do Entry.Actor:GetRootComponent():SetVisibility(true, true) end
    ApplyCharacterSelection()
    if Camera then
        Camera:SetIsOrthogonal(true)
        Camera:SetOrthoWidth(12.0)
        Camera:SetWorldLocation(GetSelectionCameraLocation(), false)
        Camera:SetWorldRotation(GameplayCameraRotation, false)
    end
    UpdateHUD()
end

-- 플레이어와 자동차의 겹침을 감지한다.
local function OnPlayerOverlap(OtherComponent)
    if GameState.Phase ~= GamePhase.Playing or GameState.PlayerIsJumping or not OtherComponent then
        return
    end

    if OtherComponent:GetName() == "CarHitbox"
        and not GameState.GameOverRequested then

        GameState.GameOverRequested = true
    end
end

function BeginPlay(Self)
    GameState.ManagedByGameManager, GameState.EnableTraffic = true, true
    Player = Actor:GetWorld():GetPlayerPawn()

    if Player then
        StartPosition = Player:GetActorLocation()
        PlayerMesh = UStaticMeshComponent.Cast(Player:FindComponentByName("StaticMeshComponent"))

        if PlayerMesh then
            PlayerMeshStartScale = PlayerMesh:GetRelativeScale3D()
            PlayerMeshStartLocation = PlayerMesh:GetRelativeLocation()
            PlayerMeshStartRotation = PlayerMesh:GetRelativeRotation()
        else
            print("[GameManager] Player StaticMeshComponent not found")
        end
        for _, Component in ipairs(Player:GetComponents()) do
            local Script = ULuaComponent.Cast(Component)
            if Script and Script:GetScriptPath() == "Scripts/Player.lua" then PlayerScript = Script end
        end
        Player:SetActorEnableCollision(true)

        PlayerHitbox = UPrimitiveComponent.Cast(Player:FindComponentByName("PlayerHitbox"))

        if PlayerHitbox then
            PlayerHitbox:SetCollisionResponseToAllChannels(ECollisionResponse.Overlap)
            BeginOverlapHandle = PlayerHitbox.OnComponentBeginOverlap:Add(OnPlayerOverlap)

            print("[GameManager] Collision ready")
        else
            print("[GameManager] PlayerHitbox not found")
        end
    end

    ScoreText = CreateHUDText("ScoreText")
    HUDTitle, HUDDetail, HUDHint = CreateHUDText("HUDTitle"), CreateHUDText("HUDDetail"), CreateHUDText("HUDHint")
    -- ATextRenderActor 생성자가 만든 기본 텍스트가 씬의 ScoreText와 중복된다.
    local DefaultText = UTextRenderComponent.Cast(Actor:FindComponentByName("TextRenderComponent"))
    if DefaultText then DefaultText:SetVisibility(false) end
    Camera = Actor:GetWorld():GetPlayerCamera()
    if Camera then GameplayCameraLocation, GameplayCameraRotation = Camera:GetWorldLocation(), Camera:GetWorldRotation() end

    if ScoreText then
        ScoreText:SetText("")
        ScoreText:SetVisibility(false)
    else
        print("[GameManager] ScoreText not found in scene")
    end

    if not Audio.PlayBGM("Audio/CrossyBGM.wav") then
        print("[GameManager] Failed to start BGM")
    end

    local Level = _G.CrossyLevel
    if Level then Level.EnableTraffic() end
    GameState.Phase, GameState.Score, GameState.MaxRow = GamePhase.Selecting, 0, 0
    GameState.GameOverRequested, GameState.ClearRequested = false, false
    CreateCharacterPreviews()
    if Camera and GameplayCameraLocation then
        Camera:SetIsOrthogonal(true)
        Camera:SetOrthoWidth(12.0)
        Camera:SetWorldLocation(GetSelectionCameraLocation(), false)
        Camera:SetWorldRotation(GameplayCameraRotation, false)
    end
    UpdateHUD()
end

function TickComponent(Self, DeltaSeconds)
    if GameState.Phase ~= GamePhase.Selecting and Input.IsKeyPressed(EKeyCode.Backspace) then
        ReturnToCharacterSelection()
        return
    end
    if GameState.Phase == GamePhase.Selecting then
        local Change = 0
        if Input.IsKeyPressed(EKeyCode.Left) or Input.IsKeyPressed(EKeyCode.A) then Change = -1
        elseif Input.IsKeyPressed(EKeyCode.Right) or Input.IsKeyPressed(EKeyCode.D) then Change = 1 end
        if Change ~= 0 then
            SelectedCharacterIndex = (SelectedCharacterIndex - 1 + Change) % #CharacterOptions + 1
            ApplyCharacterSelection()
        end
        if Input.IsKeyPressed(EKeyCode.Enter) or Input.IsKeyPressed(EKeyCode.Space) then EnterSelectedCharacter(); return end
        if Camera and GameplayCameraLocation then
            local Current, Target = Camera:GetWorldLocation(), GetSelectionCameraLocation()
            Camera:SetWorldLocation(Current + (Target - Current) * math.min(1, DeltaSeconds * CameraMoveSpeed), false)
        end
        UpdateHUD()
        return
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

    -- 선택 완료 후에는 진행 중에도 R키로 같은 캐릭터로 재시작합니다.
    if Input.IsKeyPressed(EKeyCode.R) then
        Restart()
    end

    if GameState.Phase == GamePhase.Playing and Player and StartPosition then
        local Position = Player:GetActorLocation()
        local Level = _G.CrossyLevel
        local Step = Level and Level.CellSize or CellSize
        local Row = math.max(0, math.floor((Position.X - StartPosition.X) / Step + 0.0001))

        -- P키로 현재 위치와 진행 행 확인
        if Input.IsKeyPressed(EKeyCode.P) then
            print("Start X: " .. StartPosition.X)
            print("Current X: " .. Position.X)
            print("Current Y: " .. Position.Y)
            print("Row: " .. Row)
        end

        -- 가장 멀리 전진한 행을 점수로 사용
        -- 최대 전진 행으로 점수를 관리한다. 무한 맵이 제공하는 행도 반영한다.
        local LevelRow = Level and Level.IsEndless and Level.HighestRow or 0
        local NewScore = math.max(GameState.Score or 0, GameState.MaxRow or 0, Row, LevelRow or 0)
        if NewScore > (GameState.Score or 0) then
            print("Score: " .. NewScore)
        end
        GameState.MaxRow = NewScore
        GameState.Score = NewScore

        -- 마지막 행에 도달하면 클리어 요청
        if not (Level and Level.IsEndless) and Row >= GoalRow then
            GameState.ClearRequested = true
        end
    end

    -- 맵 도착 판정 또는 외부 이벤트의 클리어 요청 처리
    if GameState.ClearRequested then
        GameState.ClearRequested = false
        GameClear()
    end

    -- 외부 LevelManager가 점수를 변경하더라도 표시를 매 프레임 동기화한다.
    UpdateHUD()
end

function EndPlay(Self)
    Audio.StopBGM()
    -- PIE 종료 시 등록한 콜백을 해제한다.
    if PlayerHitbox and BeginOverlapHandle then
        PlayerHitbox.OnComponentBeginOverlap:Remove(BeginOverlapHandle)
    end

    BeginOverlapHandle = nil
    PlayerHitbox = nil
    for _, Entry in pairs(CharacterPreviewActors) do Entry.Actor:Destroy() end
    CharacterPreviewActors = {}
    if _G.CrossyGameState == GameState then _G.CrossyGameState = nil end
end
