
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
local ScoreText = nil
local Camera = nil

local PlayerHitbox = nil
local BeginOverlapHandle = nil

local GameplayCameraLocation = nil
local GameplayCameraRotation = nil

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

local CharacterRowX = -200.0
local CharacterSpacing = 5.0
local CameraMoveSpeed = 8.0

local CellSize = 1.0
local GoalRow = 150  -- 실제 맵의 마지막 행에 맞게 수정

local function GetCharacterRowY(Index)
    return (Index - (#CharacterOptions + 1) / 2) * CharacterSpacing
end

local function GetSelectionCameraLocation()
    return FVector.new(
        CharacterRowX - 8.0,
        GetCharacterRowY(SelectedCharacterIndex),
        GameplayCameraLocation.Z
    )
end

local function CreateCharacterPreviews()
    local World = Actor:GetWorld()
    local PreviewClass = UClass.Find("AStaticMeshActor")

    if not World or not PreviewClass then
        return
    end

    for Index, Option in ipairs(CharacterOptions) do
        local Transform = FTransform.new()

        Transform.Location = FVector.new(
            CharacterRowX,
            GetCharacterRowY(Index),
            0.0
        )

        local PreviewActor = World:SpawnActor(
            PreviewClass,
            "CharacterPreview_" .. Index,
            Transform
        )

        if PreviewActor then
            PreviewActor:SetActorEnableCollision(false)

            local PreviewMesh = UStaticMeshComponent.Cast(
                PreviewActor:FindComponentByName("UStaticMeshComponent")
            )

            local MeshAsset = UAssetManager:GetAssetByPath(Option.Path)

            if PreviewMesh and MeshAsset then
                PreviewMesh:SetStaticMesh(MeshAsset)

                local BaseScale =
                    PreviewMesh:GetRelativeScale3D() * 1.35

                PreviewMesh:SetRelativeScale3D(BaseScale)

                CharacterPreviewActors[Index] = {
                    Actor = PreviewActor,
                    Mesh = PreviewMesh,
                    BaseScale = BaseScale
                }
            else
                PreviewActor:GetRootComponent():SetHiddenInGame(true, true)
                print("[CharacterSelect] Preview load failed: " .. Option.Path)
            end
        end
    end
end

local function ApplyCharacterSelection()
    for Index, Entry in pairs(CharacterPreviewActors) do
        local Scale = Entry.BaseScale

        if Index == SelectedCharacterIndex then
            Scale = Scale * 1.2
        end

        Entry.Mesh:SetRelativeScale3D(Scale)
    end
end

local function MoveSelectionLeft()
    SelectedCharacterIndex = SelectedCharacterIndex - 1

    if SelectedCharacterIndex < 1 then
        SelectedCharacterIndex = #CharacterOptions
    end

    ApplyCharacterSelection()
end

local function MoveSelectionRight()
    SelectedCharacterIndex = SelectedCharacterIndex + 1

    if SelectedCharacterIndex > #CharacterOptions then
        SelectedCharacterIndex = 1
    end

    ApplyCharacterSelection()
end

local function StartGame()
    GameState.Phase = GamePhase.Playing
    GameState.Score = 0
    GameState.MaxRow = 0
    GameState.GameOverRequested = false
    GameState.ClearRequested = false

    print("Game Started")
end

local function EnterSelectedCharacter()
    local Option = CharacterOptions[SelectedCharacterIndex]

    if not PlayerMesh then
        print("[CharacterSelect] Player mesh not found")
        return
    end

    local MeshAsset = UAssetManager:GetAssetByPath(Option.Path)

    if not MeshAsset then
        print("[CharacterSelect] Character asset not found: " .. Option.Path)
        return
    end

    PlayerMesh:SetStaticMesh(MeshAsset)

    GameState.SelectedCharacter = Option.Name

    for _, Entry in pairs(CharacterPreviewActors) do
        Entry.Actor:GetRootComponent():SetHiddenInGame(true, true)
    end

    if Camera and GameplayCameraLocation and GameplayCameraRotation then
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

    -- 플레이어를 시작 위치로 복귀
    if Player and StartPosition then
        Player:GetRootComponent():SetWorldLocation(StartPosition, false)
    end
    if PlayerMesh and PlayerMeshStartScale then
        PlayerMesh:SetRelativeScale3D(PlayerMeshStartScale)
    end
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
    Player = Actor:GetWorld():GetPlayerPawn()

    if Player then
        StartPosition = Player:GetActorLocation()
        PlayerMesh = UStaticMeshComponent.Cast(Player:FindComponentByName("StaticMeshComponent"))

        if PlayerMesh then
            PlayerMeshStartScale = PlayerMesh:GetRelativeScale3D()
        else
            print("[GameManager] Player StaticMeshComponent not found")
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

    if Camera then
        GameplayCameraLocation = Camera:GetWorldLocation()
        GameplayCameraRotation = Camera:GetWorldRotation()
    end

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

    GameState.Phase = GamePhase.Selecting
    GameState.Score = 0
    GameState.MaxRow = 0
    GameState.GameOverRequested = false
    GameState.ClearRequested = false

    CreateCharacterPreviews()
    ApplyCharacterSelection()

    if Camera and GameplayCameraLocation and GameplayCameraRotation then
        Camera:SetWorldLocation(GetSelectionCameraLocation(), false)
        Camera:SetWorldRotation(GameplayCameraRotation, false)
    end
end

function TickComponent(Self, DeltaSeconds)
    if GameState.Phase == GamePhase.Selecting then
        if Input.IsKeyPressed(EKeyCode.Left) or Input.IsKeyPressed(EKeyCode.A) then
            MoveSelectionLeft()
        elseif Input.IsKeyPressed(EKeyCode.Right) or Input.IsKeyPressed(EKeyCode.D) then
            MoveSelectionRight()
        end

        if Input.IsKeyPressed(EKeyCode.Enter) or Input.IsKeyPressed(EKeyCode.Space) then
            EnterSelectedCharacter()
            return
        end

        if Camera and GameplayCameraLocation then
            local TargetLocation = GetSelectionCameraLocation()
            local CurrentLocation = Camera:GetWorldLocation()
            local Alpha = math.min(1.0, DeltaSeconds * CameraMoveSpeed)

            local NewLocation =
                CurrentLocation + (TargetLocation - CurrentLocation) * Alpha

            Camera:SetWorldLocation(NewLocation, false)
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

    -- 게임 진행 상태와 관계없이 R키로 재시작합니다.
    if Input.IsKeyPressed(EKeyCode.R) then
        Restart()
    end

    if GameState.Phase == GamePhase.Playing and Player and StartPosition then
        local Position = Player:GetActorLocation()
        local Row = math.max(0, math.floor((Position.X - StartPosition.X) / CellSize))

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

function EndPlay(Self)
    Audio.StopBGM()
    -- PIE 종료 시 등록한 콜백을 해제한다.
    if PlayerHitbox and BeginOverlapHandle then
        PlayerHitbox.OnComponentBeginOverlap:Remove(BeginOverlapHandle)
    end

    BeginOverlapHandle = nil
    PlayerHitbox = nil
end
