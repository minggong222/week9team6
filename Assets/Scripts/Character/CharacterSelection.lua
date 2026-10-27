-- 캐릭터 목록·선택 인덱스·프리뷰는 이 스크립트에서만 관리
local Config = { RowX = -200.0, Spacing = 5.0, Size = 2.0 }
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

-- 실행 상태와 연결 참조는 HotReload 시 보존. 기본 설정은 local로 선언
State = { SelectedIndex = 1, Previews = {} }
Refs = {}

function BeginPlay(Self)
    Self:SetComponentTickEnabled(false)
end

function Initialize(Player, PlayerMesh)
    Refs.Player, Refs.PlayerMesh = Player, PlayerMesh
end

-- 원본 크기와 원점 차이에 따른 가로·세로 최대 크기 및 바닥 높이 보정
local function FitCharacterMesh(Mesh, Owner)
    Mesh:SetRelativeLocation(FVector.new(0, 0, 0))
    Mesh:SetRelativeScale3D(FVector.new(1, 1, 1))

    local Bounds, Position = Mesh:CalcBounds(), Owner:GetActorLocation()
    local Size = math.max(Bounds.Max.X - Bounds.Min.X, Bounds.Max.Y - Bounds.Min.Y)
    assert(Size > 0, "Character has no horizontal bounds")

    local Scale = Config.Size / Size
    Mesh:SetRelativeScale3D(FVector.new(Scale, Scale, Scale))
    Mesh:SetRelativeLocation(FVector.new(
        -(Bounds.Min.X + Bounds.Max.X - 2 * Position.X) * Scale / 2,
        -(Bounds.Min.Y + Bounds.Max.Y - 2 * Position.Y) * Scale / 2,
        -(Bounds.Min.Z - Position.Z) * Scale))
    return Mesh:GetRelativeScale3D()
end

local function GetCharacterRowY(Index)
    return (Index - (#CharacterOptions + 1) / 2) * Config.Spacing
end

local function HighlightSelection()
    for Index, Entry in pairs(State.Previews) do
        Entry.Mesh:SetRelativeScale3D(Entry.BaseScale * (Index == State.SelectedIndex and 1.2 or 1.0))
    end
end

function GetSelection()
    return CharacterOptions[State.SelectedIndex].Name, State.SelectedIndex, #CharacterOptions
end

function GetSelectionCameraLocation()
    return FVector.new(Config.RowX - 8, GetCharacterRowY(State.SelectedIndex), 8.0)
end

-- 프리뷰는 한 번만 생성. 플레이·선택 전환 시 표시 여부만 변경
function CreatePreviews()
    local World = Actor:GetWorld()
    for Index, Option in ipairs(CharacterOptions) do
        local Pose = FTransform.new()
        Pose.Location = FVector.new(Config.RowX, GetCharacterRowY(Index), 0)
        local Preview = World:SpawnActor(UClass.Find("AStaticMeshActor"), "CharacterPreview_" .. Index, Pose)
        assert(Preview, "Could not create character preview")
        Preview:SetActorEnableCollision(false)

        local Mesh = UStaticMeshComponent.Cast(Preview:FindComponentByName("UStaticMeshComponent"))
        local Asset = UAssetManager:GetAssetByPath(Option.Path)
        if Mesh and Asset then
            Mesh:SetStaticMesh(Asset)
            State.Previews[Index] = { Actor = Preview, Mesh = Mesh, BaseScale = FitCharacterMesh(Mesh, Preview) }
        else
            Preview:Destroy()
            print("[CharacterSelect] Missing preview: " .. Option.Path)
        end
    end
    HighlightSelection()
end

-- 입력 결과만 반환. HUD·카메라·게임 단계 변경은 GameManager 담당
function HandleInput()
    local Change = 0
    if Input.IsKeyPressed(EKeyCode.Left) or Input.IsKeyPressed(EKeyCode.A) then
        Change = -1
    elseif Input.IsKeyPressed(EKeyCode.Right) or Input.IsKeyPressed(EKeyCode.D) then
        Change = 1
    end

    if Change ~= 0 then
        State.SelectedIndex = (State.SelectedIndex - 1 + Change) % #CharacterOptions + 1
        HighlightSelection()
    end
    local Confirmed = Input.IsKeyPressed(EKeyCode.Enter) or Input.IsKeyPressed(EKeyCode.Space)
    return Change ~= 0, Confirmed
end

function GetSelectedAsset()
    local Preview = State.Previews[State.SelectedIndex]
    return Preview and Preview.Mesh:GetStaticMesh() or UAssetManager:GetAssetByPath(CharacterOptions[State.SelectedIndex].Path)
end

function ApplySelectedCharacter(Asset)
    Refs.PlayerMesh:SetStaticMesh(Asset)
    FitCharacterMesh(Refs.PlayerMesh, Refs.Player)
end

function ShowPreviews()
    for _, Entry in pairs(State.Previews) do
        Entry.Actor:GetRootComponent():SetVisibility(true, true)
    end
    HighlightSelection()
end

function HidePreviews()
    for _, Entry in pairs(State.Previews) do
        Entry.Actor:GetRootComponent():SetVisibility(false, true)
    end
end

function DestroyPreviews()
    for _, Entry in pairs(State.Previews) do Entry.Actor:Destroy() end
    State.Previews = {}
end
