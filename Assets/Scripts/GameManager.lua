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
local ScoreLabel = nil
local SelectionTitle, SelectionName, SelectionCount, SelectionHint = nil, nil, nil, nil
local ResultTitle, ResultScore, ResultHint = nil, nil, nil
local Camera = nil

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
    return FVector.new(CharacterRowX - 8, GetCharacterRowY(SelectedCharacterIndex), GameplayCameraLocation.Z)
end

-- 기존 ScoreText는 재사용하고, 추가 UI는 게임 매니저에 생성한다.
local function CreateUIText(Name, Text, Size)
    local Component = UTextRenderComponent.Cast(Actor:FindComponentByName(Name))

    if not Component then
        Component = UTextRenderComponent.Cast(Actor:AddComponent(UTextRenderComponent.StaticClass(), Name))
    end

    if not Component then
        print("[GameManager] UI component not found: " .. Name)
        return nil
    end

    Component:SetText(Text)
    Component:SetTextSize(Size)
    Component:SetVisibility(false)
    return Component
end

local function SetUITextVisible(Component, bVisible)
    if Component then
        Component:SetVisibility(bVisible)
    end
end

local function UpdateSelectionUIText()
    local Option = CharacterOptions[SelectedCharacterIndex]

    if SelectionName then
        SelectionName:SetText("<  " .. Option.Name:upper() .. "  >")
    end

    if SelectionCount then
        SelectionCount:SetText(string.format("%02d / %02d", SelectedCharacterIndex, #CharacterOptions))
    end
end

-- 선택 / 플레이 / 결과 화면의 텍스트를 각각 표시한다.
local function ShowUIForPhase(Phase)
    local bSelecting = Phase == GamePhase.Selecting
    local bPlaying = Phase == GamePhase.Playing
    local bResult = Phase == GamePhase.Dead or Phase == GamePhase.Clear

    SetUITextVisible(SelectionTitle, bSelecting)
    SetUITextVisible(SelectionName, bSelecting)
    SetUITextVisible(SelectionCount, bSelecting)
    SetUITextVisible(SelectionHint, bSelecting)

    SetUITextVisible(ScoreLabel, bPlaying)
    SetUITextVisible(ScoreText, bPlaying)

    SetUITextVisible(ResultTitle, bResult)
    SetUITextVisible(ResultScore, bResult)
    SetUITextVisible(ResultHint, bResult)
end

-- 3D 텍스트를 카메라 앞에 배치해서 화면 UI처럼 보이게 한다.
local function UpdateUIPosition()
    if not Camera then
        return
    end

    local Rotation = Camera:GetWorldRotation()
    local Quaternion = Rotation:Quaternion()
    local Up = Quaternion:GetUpVector()
    local BaseLocation = Camera:GetWorldLocation() + Quaternion:GetForwardVector() * 4.0

    local function Place(Component, VerticalOffset)
        if Component then
            Component:SetWorldLocation(BaseLocation + Up * VerticalOffset, false)
            Component:SetWorldRotation(Rotation, false)
        end
    end

    if GameState.Phase == GamePhase.Selecting then
        Place(SelectionTitle, 1.20)
        Place(SelectionName, -0.48)
        Place(SelectionCount, -0.89)
        Place(SelectionHint, -1.20)
    elseif GameState.Phase == GamePhase.Playing then
        Place(ScoreLabel, 1.26)
        Place(ScoreText, 0.80)
    elseif GameState.Phase == GamePhase.Dead or GameState.Phase == GamePhase.Clear then
        Place(ResultTitle, 0.50)
        Place(ResultScore, -0.06)
        Place(ResultHint, -0.55)
    end
end

local function ApplyCharacterSelection()
    for Index, Entry in pairs(CharacterPreviewActors) do
        Entry.Mesh:SetRelativeScale3D(Entry.BaseScale * (Index == SelectedCharacterIndex and 1.2 or 1.0))
    end
    GameState.PreviewCharacter = CharacterOptions[SelectedCharacterIndex].Name
    UpdateSelectionUIText()
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

    if ScoreText then
        ScoreText:SetText("000")
    end
    ShowUIForPhase(GamePhase.Playing)

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
    if Camera and GameplayCameraLocation then
        Camera:SetWorldLocation(GameplayCameraLocation, false)
        Camera:SetWorldRotation(GameplayCameraRotation, false)
    end
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

    if ResultTitle then
        ResultTitle:SetText("GAME OVER")
    end
    if ResultScore then
        ResultScore:SetText("SCORE  " .. GameState.Score)
    end
    ShowUIForPhase(GamePhase.Dead)

    print("Game Over!")
end

-- 장애물에서도 UI를 포함한 동일한 게임 오버 처리를 사용합니다.
GameState.GameOver = GameOver

local function GameClear()
    if GameState.Phase ~= GamePhase.Playing then
        return
    end

    GameState.Phase = GamePhase.Clear

    if ResultTitle then
        ResultTitle:SetText("GAME CLEAR")
    end
    if ResultScore then
        ResultScore:SetText("SCORE  " .. GameState.Score)
    end
    ShowUIForPhase(GamePhase.Clear)

    print("Game Clear!")
end

local function Restart()
    GameState.RestartCount = (GameState.RestartCount or 0) + 1

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
    StartGame()

    -- Player.lua에서 진행 중인 점프와 방향 초기화
    GameState.PlayerResetRequested = true

end

-- 플레이어와 자동차의 겹침을 감지한다.
local function OnPlayerOverlap(OtherComponent)
    if GameState.Phase ~= GamePhase.Playing or not OtherComponent then
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

    ScoreText = UTextRenderComponent.Cast(Actor:FindComponentByName("ScoreText"))
    Camera = Actor:GetWorld():GetPlayerCamera()
    if Camera then GameplayCameraLocation, GameplayCameraRotation = Camera:GetWorldLocation(), Camera:GetWorldRotation() end

    if not ScoreText then
        ScoreText = CreateUIText("ScoreText", "000", 0.42)
    else
        ScoreText:SetText("000")
        ScoreText:SetTextSize(0.42)
        ScoreText:SetVisibility(false)
    end

    ScoreLabel = CreateUIText("ScoreLabel", "SCORE", 0.19)
    SelectionTitle = CreateUIText("SelectionTitle", "CHOOSE YOUR CHARACTER", 0.31)
    SelectionName = CreateUIText("SelectionName", "", 0.47)
    SelectionCount = CreateUIText("SelectionCount", "", 0.22)
    SelectionHint = CreateUIText("SelectionHint", "LEFT / RIGHT : SELECT\nENTER : PLAY", 0.18)
    ResultTitle = CreateUIText("ResultTitle", "", 0.56)
    ResultScore = CreateUIText("ResultScore", "", 0.32)
    ResultHint = CreateUIText("ResultHint", "PRESS R TO RETRY", 0.21)

    if not Audio.PlayBGM("Audio/CrossyBGM.wav") then
        print("[GameManager] Failed to start BGM")
    end

    local Level = _G.CrossyLevel
    if Level then Level.EnableTraffic() end
    GameState.Phase, GameState.Score, GameState.MaxRow = GamePhase.Selecting, 0, 0
    GameState.GameOverRequested, GameState.ClearRequested = false, false
    CreateCharacterPreviews()
    if Camera and GameplayCameraLocation then
        Camera:SetWorldLocation(GetSelectionCameraLocation(), false)
        Camera:SetWorldRotation(GameplayCameraRotation, false)
    end
    ShowUIForPhase(GamePhase.Selecting)
    UpdateUIPosition()
end

function TickComponent(Self, DeltaSeconds)
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
        UpdateUIPosition()
        return
    end
    UpdateUIPosition()

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
        if Row > GameState.MaxRow then
            GameState.MaxRow = Row
            GameState.Score = Row

            if ScoreText then
                ScoreText:SetText(string.format("%03d", GameState.Score))
            end

            print("Score: " .. GameState.Score)
        end

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
