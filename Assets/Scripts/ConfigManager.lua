-- 게임 설정의 단일 편집 위치. 각 로드는 독립된 테이블을 반환해 실행 중 변경이 다른 세션에 남지 않는다.
-- 조명 색상은 RGB/RGBA(0~255), 위치/범위는 월드 단위를 사용한다.
local ConfigManager = {}

ConfigManager.Level = {
    NightMode = false, -- 최초 선택 모드
    Spotlight = {
        ShadowSlopeBias = 0.05,
        InnerConeAngle = 30,
        OuterConeAngle = 40
    },
    -- 낮/밤별 조명 설정. Color는 0~255 RGB 또는 RGBA를 사용한다.
    Lighting = {
        Day = {
            Sky = { Intensity = 0.2, Color = { 255, 255, 255 } },
            Sun = { Intensity = 0.5, Color = { 255, 255, 255 } },
            Car = { Intensity = 0, Color = { 255, 235, 195 }, AttenuationRadius = 15 },
            Train = { Intensity = 0, Color = { 220, 235, 255 } }
        },
        Night = {
            Sky = { Intensity = 0.1, Color = { 117, 109, 176 } },
            Sun = { Intensity = 0.5, Color = { 132, 143, 227 } },
            Car = { Intensity = 4, Color = { 255, 235, 195 }, AttenuationRadius = 15 },
            Train = { Intensity = 18, Color = { 220, 235, 255 } }
        }
    },
    CellSize = 3.0,
    RowsBehind = 10,
    RowsAhead = 30,
    DeathRowsBehind = 6,
    HalfWidth = 24.0,
    BoundaryWidth = 0.3,
    RoadHalfWidth = 34.0,
    InitialSeed = 20261026
}

ConfigManager.Game = { CellSize = ConfigManager.Level.CellSize, GoalRow = 150 }

ConfigManager.Traffic = {
    Day = {
        MinSpeed = 8.0,
        MaxSpeed = 20.0,
        MinSpawnInterval = 0.25,
        MaxSpawnInterval = 2.5
    },
    Night = {
        MinSpeed = 10.8,
        MaxSpeed = 27.0,
        MinSpawnInterval = 0.25,
        MaxSpawnInterval = 1.75
    },
    Headlight = {
        SideOffsetRatio = 0.32,
        Height = 0.8,
        Pitch = 12,
        Yaw = 180,
        CastShadows = false
    },
    PoolCount = 100,
    CarWidth = 2.0,
    MinCarGap = 0.8,
    MaxInitialCarsPerRow = 4,
    PositiveYChance = 0.50,
    Cars = {
        { Path = "Models/Crossy/Cars/taxi.obj", Weight = 1.0 },
        { Path = "Models/Crossy/Cars/sedan.obj", Weight = 1.0 },
        { Path = "Models/Crossy/Cars/truck.obj", Weight = 1.0 },
        { Path = "Models/Crossy/Cars/firetruck.obj", Weight = 1.0 },
        { Path = "Models/Crossy/Cars/garbage-truck.obj", Weight = 1.0 }
    }
}

ConfigManager.Train = {
    Day = {
        Speed = 50.0,
        WaitMin = 3.0,
        WaitMax = 6.0,
        WarningTime = 1.0,
        InitialWaitMin = 1.0,
        InitialWaitMax = 4.0
    },
    Night = {
        Speed = 65.0,
        WaitMin = 2.25,
        WaitMax = 4.5,
        WarningTime = 1.0,
        InitialWaitMin = 1.0,
        InitialWaitMax = 4.0
    },
    Headlight = {
        SideOffsetRatio = 0.32,
        Height = 1.8,
        Pitch = 10,
        Yaw = 90,
        Radius = 26,
        CastShadows = false
    },
    BlinkDuration = 1.0,
    BlinkInterval = 0.25,
    SignalSpacing = 12.0,
    SignalOffsetX = -1.4,
    SignalIntensity = 6.0,
    PassSoundRange = 18.0,
    PassSoundLead = 30.0,
    PassSoundVolume = 0.70,
    WarningSoundVolume = 0.50,
    Types = {
        { Name = "bullet", Path = "Models/Crossy/Trains/crossy-train-long.obj" },
        { Name = "city", Path = "Models/Crossy/Trains/crossy-train-city.obj" },
        { Name = "freight", Path = "Models/Crossy/Trains/crossy-train-freight.obj" }
    }
}

ConfigManager.Map = {
    Day = {
        RoadChance = 0.60,
        RailChance = 0.05,
        TreesPerRow = 5,
        TreeWeights = { 5, 15, 20, 30, 20, 10 }
    },
    Night = {
        RoadChance = 0.60,
        RailChance = 0.05,
        TreesPerRow = 5,
        TreeWeights = { 5, 15, 20, 30, 20, 10 }
    },
    TreeHitboxExtent = {
        X = 0.45,
        Y = 0.45,
        Z = 1.0
    }
}

ConfigManager.Camera = {
    Pitch = 40.0,
    Yaw = -35.0,
    Width = 36.0,
    Distance = 32.0,
    LookAhead = 5.0,
    MoveSpeed = 8.0,
    SelectionWidth = 12.0
}

ConfigManager.HUD = {
    ScorePulseDuration = 0.20,
    ScorePulseAmount = 0.2,
    GameOverSwingPeriod = 2.0,
    GameOverSwingAngle = 7.0,
    Scale = 1.0,
    Score = 0.105,
    Title = 0.032,
    Name = 0.052,
    Hint = 0.023
}

ConfigManager.Selection = {
    RowX = -200.0,
    Spacing = 5.0,
    Size = 2.0
}

ConfigManager.Characters = {
    { Name = "Hitori", Path = "Models/OBJ format/animal-hitori.obj" },
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
}

ConfigManager.HitReaction = { SquashRatio = 0.05, SquashDuration = 0.12 }

ConfigManager.Player = {
    MoveDistance = 3.0,
    MinY = -33,
    MaxY = 33,
    YawOffset = 180.0,
    JumpDuration = 0.1,
    JumpHeight = 0.5,
    JumpSquashRatio = 0.12,
    JumpSquashDuration = 0.08,
    LandingSquashDuration = 0.08,
    BounceDuration = 0.12,
    CollisionStep = 0.01,
    TreeHitboxExtent = {
        X = 1.0,
        Y = 1.0,
        Z = 0.5
    },
}

ConfigManager.Cinematics = {
    FadeDuration = 0.15,
    LetterBoxHeight = 0.08,
    LetterBoxDuration = 0.25,
    Black = { 0, 0, 0, 255 },
    DeathZoom = {
        Scale = 0.9,
        InDuration = 0.25,
        OutDuration = 0.15
    },
    DeathShake = {
        Duration = 0.35,
        Location = 1.5,
        Rotation = 1.25,
        Frequency = 8.0
    },
    ModeShake = {
        Duration = 0.25,
        Location = 0.15,
        Rotation = 0.8,
        Frequency = 18
    },
    HitStop = {
        Scale = 0,
        Hold = 0.25,
        Recovery = 1.5
    }
}

ConfigManager.Game.DefaultMode = ConfigManager.Level.NightMode and "Night" or "Day"
ConfigManager.Game.ModeShake = ConfigManager.Cinematics.ModeShake
ConfigManager.HUD.DefaultMode = ConfigManager.Game.DefaultMode

return ConfigManager
