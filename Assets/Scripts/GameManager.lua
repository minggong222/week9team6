
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
    { Name = "Tiger", Path = "Models/OBJ format/animal-tiger.obj" }
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

local function ApplyCharacterSelection()
    for Index, Entry in pairs(CharacterPreviewActors) do
        Entry.Mesh:SetRelativeScale3D(Entry.BaseScale * (Index == SelectedCharacterIndex and 1.2 or 1.0))
    end
    GameState.PreviewCharacter = CharacterOptions[SelectedCharacterIndex].Name
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
    GameState.GameOverRequested = false
    GameState.ClearRequested = false

    if ScoreText then
        ScoreText:SetVisibility(true)
        ScoreText:SetText("Score: 0")
    end

    print("Game Started")
end

local function EnterSelectedCharacter()
    local Option = CharacterOptions[SelectedCharacterIndex]
    local Asset = UAssetManager:GetAssetByPath(Option.Path)
    if not PlayerMesh or not Asset then print("[CharacterSelect] Cannot select " .. Option.Name); return end
    if PlayerScript then PlayerScript:Invoke("StopMovement") end
    PlayerMesh:SetStaticMesh(Asset)
    FitCharacterMesh(PlayerMesh, Player)
    PlayerMeshStartScale, PlayerMeshStartLocation = PlayerMesh:GetRelativeScale3D(), PlayerMesh:GetRelativeLocation()
    PlayerMeshStartRotation = PlayerMesh:GetRelativeRotation()
    if PlayerScript then PlayerScript:Invoke("RefreshMeshBaseLocation") end
    GameState.SelectedCharacter = Option.Name
    for _, Entry in pairs(CharacterPreviewActors) do Entry.Actor:GetRootComponent():SetVisibility(false, true) end
    if Camera and GameplayCameraLocation then
        Camera:SetWorldLocation(GameplayCameraLocation, false)
        Camera:SetWorldRotation(GameplayCameraRotation, false)
    end
    StartGame()
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

    if ScoreText then
        ScoreText:SetText("GAME OVER\nScore: " .. GameState.Score .. "\nPress R to Restart")
    end

    print("Game Over!")
end

-- 장애물에서도 UI를 포함한 동일한 게임 오버 처리를 사용합니다.
GameState.GameOver = GameOver

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
    GameState.RestartCount = (GameState.RestartCount or 0) + 1
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

    if ScoreText then
        ScoreText:SetText("Score: 0")
    end
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

    if ScoreText then
        ScoreText:SetText("Score: 0")
        ScoreText:SetTextSize(0.25)
        ScoreText:SetVisibility(false)
    else
        print("[GameManager] ScoreText not found")
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
        Camera:SetWorldLocation(GetSelectionCameraLocation(), false)
        Camera:SetWorldRotation(GameplayCameraRotation, false)
    end
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
        return
    end
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
                ScoreText:SetText("Score: " .. GameState.Score)
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
