-- HUD에 표시할 값만 전달받아 사용. 게임 단계·캐릭터 목록의 직접 조회 제외
local Config = dofile("Assets/Scripts/ConfigManager.lua").HUD
local ScorePulse = { Duration = Config.ScorePulseDuration, Amount = Config.ScorePulseAmount, Elapsed = Config.ScorePulseDuration }
local GameOverSwing = { Period = Config.GameOverSwingPeriod, Angle = Config.GameOverSwingAngle, Elapsed = 0, Active = false }
Refs = {}
local Presentation = { Elapsed = 0 }

local function SetPresentation(Phase, Score, HUDScale, HiScore, NewRecord)
    if NewRecord and not Presentation.NewRecord then Presentation.Elapsed = 0 end
    Presentation.Phase, Presentation.Score, Presentation.HUDScale = Phase, Score, HUDScale
    Presentation.HiScore, Presentation.NewRecord = HiScore or 0, NewRecord or false
end

local function UpdateScoreColor()
    local Color = { 255, 255, 255 }
    if Presentation.NewRecord then
        local Position = Presentation.Elapsed / Config.RainbowPeriod * #Config.RecordColors
        local Index = math.floor(Position) % #Config.RecordColors + 1
        local Next = Index % #Config.RecordColors + 1
        local Alpha = Position - math.floor(Position)
        for Channel = 1, 3 do
            local Start = Config.RecordColors[Index][Channel]
            Color[Channel] = math.floor(Start + (Config.RecordColors[Next][Channel] - Start) * Alpha + 0.5)
        end
    end
    if Refs.ScoreText then Refs.ScoreText:SetTextColor(FColor.new(Color[1], Color[2], Color[3], 255)) end
end

local function HideRecordDisplay()
    SetPresentation(nil, 0, nil, 0, false)
    for _, Name in ipairs({ "HiScoreTitle", "HiScoreText", "RecordText" }) do
        if Refs[Name] then Refs[Name]:SetVisibility(false) end
    end
    UpdateScoreColor()
end

function BeginPlay(Self)
    Refs.Self = Self
    Self:SetComponentTickEnabled(false)
end

local function ResetScorePulse()
    ScorePulse.Score = nil
    ScorePulse.Elapsed = ScorePulse.Duration
    if Refs.Self then Refs.Self:SetComponentTickEnabled(false) end
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

function Initialize(Camera)
    Refs.Camera = Camera
    Refs.ScoreText = CreateHUDText("ScoreText")
    Refs.HUDTitle = CreateHUDText("HUDTitle")
    Refs.HUDDetail = CreateHUDText("HUDDetail")
    Refs.HUDHint = CreateHUDText("HUDHint")
    Refs.HiScoreTitle = CreateHUDText("HiScoreTitle")
    Refs.HiScoreText = CreateHUDText("HiScoreText")
    Refs.RecordText = CreateHUDText("RecordText")

    -- Actor 생성자의 기본 텍스트와 씬의 ScoreText 중복 표시 방지
    local DefaultText = UTextRenderComponent.Cast(Actor:FindComponentByName("TextRenderComponent"))
    if DefaultText then DefaultText:SetVisibility(false) end

    if Refs.ScoreText then
        Refs.ScoreText:SetText("")
        Refs.ScoreText:SetVisibility(false)
    else
        print("[GameManager] ScoreText not found in scene")
    end
end

-- 화면 비율 기반 배치로 직교·원근 투영 및 창 종횡비 변경 시에도 같은 구도 유지
local function BeginLayout(HUDScale, ViewCamera)
    local Camera = ViewCamera or Refs.Camera
    if not Refs.ScoreText or not Camera then return nil end
    local Distance = 2.0
    local Aspect = math.max(Camera:GetAspectRatio(), 0.1)
    local Height = Camera:GetIsOrthogonal() and Camera:GetOrthoWidth() / Aspect
        or 2 * Distance * math.tan(math.rad(Camera:GetFieldOfView()) / 2)
    local Width = Height * Aspect

    local Rotation = Camera:GetWorldRotation()
    local Q = Rotation:Quaternion()
    local Center = Camera:GetWorldLocation() + Q:GetForwardVector() * Distance
    local Scale = math.max(0.5, math.min(1.5, HUDScale or Config.Scale))

    local function Place(Component, Text, Size, X, Y)
        if not Component then return end
        if Component:GetText() ~= Text then Component:SetText(Text) end
        Component:SetTextSize(Height * Size * Scale)
        Component:SetWorldLocation(Center + Q:GetRightVector() * ((X - 0.5) * Width)
            + Q:GetUpVector() * ((Y - 0.5) * Height), false)
        Component:SetWorldRotation(Rotation, false)
        Component:SetVisibility(Text ~= "")
    end

    return Place
end

function ShowSelection(Name, Index, Count, HUDScale, Mode)
    HideRecordDisplay()
    GameOverSwing.Active = false
    ResetScorePulse()
    local Place = BeginLayout(HUDScale)
    if not Place then return end

    Place(Refs.HUDTitle, "CHOOSE YOUR CHARACTER", Config.Title, 0.5, 0.88)
    Place(Refs.ScoreText, "<  " .. Name:upper() .. "  >", Config.Name, 0.5, 0.25)
    Place(Refs.HUDDetail, string.format("%02d / %02d     |     %s MODE", Index, Count, (Mode or Config.DefaultMode):upper()), Config.Hint, 0.5, 0.18)
    Place(Refs.HUDHint, "A/D, LEFT/RIGHT : CHARACTER     W/S, UP/DOWN : MODE     ENTER : PLAY", Config.Hint, 0.5, 0.09)
end

function ShowScore(Score, HUDScale, HiScore, NewRecord)
    NewRecord = NewRecord or false
    GameOverSwing.Active = false
    local Place = BeginLayout(HUDScale)
    if not Place then return end

    Score = Score or 0
    SetPresentation("Playing", Score, HUDScale, HiScore, NewRecord)
    UpdateScoreColor()
    if ScorePulse.Score and Score > ScorePulse.Score then
        -- 연속 득점 시에도 원래 크기 기준으로 연출을 다시 시작한다.
        ScorePulse.Elapsed = 0
        if Refs.Self then Refs.Self:SetComponentTickEnabled(true) end
    elseif ScorePulse.Score and Score < ScorePulse.Score then
        ResetScorePulse()
    end
    ScorePulse.Score, ScorePulse.HUDScale = Score, HUDScale

    local Progress = math.min(ScorePulse.Elapsed / ScorePulse.Duration, 1)
    local Scale = 1 + ScorePulse.Amount * math.sin(math.pi * Progress)
    local Columns = Config.ScoreColumns.Playing
    Place(Refs.HUDTitle, "SCORE", Config.Hint, Columns.Score, 0.92)
    Place(Refs.ScoreText, string.format("%03d", Score), Config.PlayingScoreSize * Scale, Columns.Score, 0.81)
    Place(Refs.RecordText, NewRecord and "New Record!" or "", Config.Hint, Columns.Score, 0.76)
    Place(Refs.HUDDetail, "HI-SCORE", Config.Hint, Columns.HiScore, 0.92)
    Place(Refs.HiScoreText, string.format("%03d", HiScore or 0), Config.PlayingScoreSize, Columns.HiScore, 0.81)
    if Refs.HiScoreTitle then Refs.HiScoreTitle:SetVisibility(false) end
    if Refs.Self then
        Refs.Self:SetComponentTickEnabled(NewRecord or ScorePulse.Elapsed < ScorePulse.Duration)
    end
    Place(Refs.HUDHint, (Score or 0) == 0 and "BACKSPACE : CHARACTERS     C : CREDITS" or "", 0.020, 0.5, 0.06)
end

function TickComponent(Self, DeltaSeconds)
    local RealDeltaSeconds = Actor:GetWorld():GetRealDeltaSeconds()
    Presentation.Elapsed = (Presentation.Elapsed + RealDeltaSeconds) % Config.RainbowPeriod
    if Presentation.Phase == "Dead" or Presentation.Phase == "Clear" then
        if GameOverSwing.Active then
            GameOverSwing.Elapsed = (GameOverSwing.Elapsed + RealDeltaSeconds) % GameOverSwing.Period
        end
        ShowResult(Presentation.Phase, Presentation.Score, Presentation.HUDScale,
            Presentation.HiScore, Presentation.NewRecord)
    elseif Presentation.Phase == "Playing" then
        ScorePulse.Elapsed = math.min(ScorePulse.Elapsed + DeltaSeconds, ScorePulse.Duration)
        ShowScore(Presentation.Score, Presentation.HUDScale, Presentation.HiScore, Presentation.NewRecord)
    else
        Self:SetComponentTickEnabled(false)
    end
end

function ShowResult(Phase, Score, HUDScale, HiScore, NewRecord)
    NewRecord = NewRecord or false
    SetPresentation(Phase, Score, HUDScale, HiScore, NewRecord)
    UpdateScoreColor()
    ResetScorePulse()
    local IsGameOver = Phase == "Dead"
    if IsGameOver and not GameOverSwing.Active then GameOverSwing.Elapsed = 0 end
    GameOverSwing.Active = IsGameOver
    GameOverSwing.Score, GameOverSwing.HUDScale = Score, HUDScale
    if Refs.Self then Refs.Self:SetComponentTickEnabled(IsGameOver or NewRecord) end

    local Place = BeginLayout(HUDScale)
    if not Place then return end

    Place(Refs.HUDTitle, IsGameOver and "GAME OVER" or "GAME CLEAR", 0.08, 0.5, 0.82)
    if IsGameOver and Refs.HUDTitle then
        -- 카메라의 시선 축을 기준으로 기울여 화면 중앙에서 좌우로 흔들리게 한다.
        local Rotation = Refs.Camera:GetWorldRotation()
        local Angle = -GameOverSwing.Angle * math.cos(2 * math.pi * GameOverSwing.Elapsed / GameOverSwing.Period)
        Refs.HUDTitle:SetWorldRotation(FRotator.new(Rotation.Pitch, Rotation.Yaw, Rotation.Roll + Angle), false)
    end
    local Columns = Config.ScoreColumns.Result
    Place(Refs.HUDDetail, "SCORE", Config.Hint, Columns.Score, 0.65)
    Place(Refs.ScoreText, string.format("%03d", Score or 0), Config.Score * 0.8, Columns.Score, 0.56)
    Place(Refs.HiScoreTitle, "HI-SCORE", Config.Hint, Columns.HiScore, 0.65)
    Place(Refs.HiScoreText, string.format("%03d", HiScore or 0), Config.Score * 0.8, Columns.HiScore, 0.56)
    Place(Refs.RecordText, NewRecord and "New Record!" or "", Config.Hint, Columns.Score, 0.51)
    Place(Refs.HUDHint, Phase == "Dead" and "R : RETRY\n\nBACKSPACE : CHARACTERS\n\nC : CREDITS" or "R : RETRY",
        0.020, 0.5, 0.32)
end

function ShowCredits(HUDScale, Camera)
    HideRecordDisplay()
    GameOverSwing.Active = false
    ResetScorePulse()
    local Place = BeginLayout(HUDScale, Camera)
    if not Place then return end

    Place(Refs.HUDTitle, "만든 사람들", 0.050, 0.5, 0.80)
    Place(Refs.ScoreText, "유현서\n\n김경재\n\n김민곤\n\n현동윤", 0.036, 0.5, 0.62)
    Place(Refs.HUDDetail, "Krafton Jungle Game Tech Lab 4기\nWeek9 Team6", 0.016, 0.5, 0.74)
    Place(Refs.HUDHint, "C / BACKSPACE : BACK", 0.020, 0.5, 0.14)
end

function Hide()
    HideRecordDisplay()
    GameOverSwing.Active = false
    ResetScorePulse()
    if not Refs.ScoreText or not Refs.Camera then return end
    Refs.ScoreText:SetVisibility(false)
    if Refs.HUDTitle then Refs.HUDTitle:SetVisibility(false) end
    if Refs.HUDDetail then Refs.HUDDetail:SetVisibility(false) end
    if Refs.HUDHint then Refs.HUDHint:SetVisibility(false) end
end
