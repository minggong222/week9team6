-- 카메라 효과와 음향 연출 진입점. 게임 규칙과 상태 전환은 GameManager 담당
local Config = dofile("Assets/Scripts/ConfigManager.lua").Cinematics
local Black = FColor.new(Config.Black[1], Config.Black[2], Config.Black[3], Config.Black[4])
State = {}


-- 게임 연출 및 카메라 시퀀스

local Sequence = {
    bPlaying = false,
    Elapsed = 0.0,
    Duration = 3.0,

    CineActor = nil,
    ReturnCamera = nil
}

local ScriptComponent = nil

function BeginPlay(Self)
    ScriptComponent = Self
    Self:SetComponentTickEnabled(false)
end

function PlayStart()
    if not Audio.PlayBGM("Audio/CrossyBGM.wav") then
        print("[GameCinematics] Failed to start BGM")
    end
end

function PlayDeath()
    local World = Actor:GetWorld()
    local CameraManager = World:GetPlayerCameraManager()
    if CameraManager then
        CameraManager:StartCameraZoom(Config.DeathZoom.Scale, Config.DeathZoom.InDuration, Config.DeathZoom.OutDuration)
        CameraManager:StartCameraShake(Config.DeathShake.Duration, Config.DeathShake.Location,
            Config.DeathShake.Rotation, Config.DeathShake.Frequency)
    end

    World:PlayHitStop(Config.HitStop.Scale, Config.HitStop.Hold, Config.HitStop.Recovery)

    Audio.PlaySFX("Audio/Splat.wav")
end

function FadeIn(Duration, Color)
    local CameraManager = Actor:GetWorld():GetPlayerCameraManager()
    return CameraManager and CameraManager:StartCameraFade(1, 0, Duration or Config.FadeDuration, Color or Black) or false
end

function FadeOut(Duration, Color)
    local CameraManager = Actor:GetWorld():GetPlayerCameraManager()
    return CameraManager and CameraManager:StartCameraFade(CameraManager:GetFadeAmount(), 1,
        Duration or Config.FadeDuration, Color or Black) or false
end

function SetLetterBox(Height, Duration)
    local CameraManager = Actor:GetWorld():GetPlayerCameraManager()
    return CameraManager and CameraManager:SetLetterBox(Height, Duration or Config.LetterBoxDuration) or false
end

function PlayRestart()
    if not FadeOut(Config.FadeDuration) then return 0 end
    return Config.FadeDuration
end

function StopEffects(KeepZoom)
    local CameraManager = Actor:GetWorld():GetPlayerCameraManager()
    if CameraManager then
        if not KeepZoom then CameraManager:StopCameraZoom() end
        CameraManager:StopCameraFade()
        CameraManager:StopLetterBox()
    end
end

function Update(Phase)
    local CameraManager = Actor:GetWorld():GetPlayerCameraManager()
    -- BeginPlay 호출 순서상 Manager가 아직 없으면 다음 Update에서 다시 확인한다.
    if not CameraManager or State.Phase == Phase then return end

    local FromAlpha = CameraManager:GetFadeAmount()
    if State.Phase == nil then FromAlpha = 1 end

    if Phase == "Playing" then
        -- 재시작 대기가 끝난 프레임은 완전 암전에서 새 화면을 드러낸다.
        if State.Phase == "Ready" then FromAlpha = 1 end
        CameraManager:StartCameraFade(FromAlpha, 0, Config.FadeDuration, Black)
        CameraManager:SetLetterBox(0, Config.LetterBoxDuration)
    elseif Phase == "SelectingCharacter" then
        CameraManager:StartCameraFade(FromAlpha, 0, Config.FadeDuration, Black)
        CameraManager:SetLetterBox(Config.LetterBoxHeight, Config.LetterBoxDuration)
    else
        -- 사망 화면 갱신은 PlayDeath 직후이므로 진행 중인 줌을 유지한다.
        StopEffects(Phase == "Dead")
    end
    State.Phase = Phase
end

function StopCinematics()
    if Sequence.bPlaying then
        local World = Actor:GetWorld()

        Sequence.bPlaying = false

        if World then
            World:EndSlomo(0.0, ETimeMode.RealTime)

            if Sequence.ReturnCamera then
                World:SetMainCameraComponent(
                    Sequence.ReturnCamera,
                    0.5
                )
            end
        end

        Sequence.ReturnCamera = nil

        if ScriptComponent then
            ScriptComponent:SetComponentTickEnabled(false)
        end
    end

    Audio.StopBGM()
    StopEffects()
end

-- F4 테스트에서 호출할 카메라 시퀀스
function PlayCameraSequence(Duration)
    if Sequence.bPlaying then
        return false
    end

    local World = Actor:GetWorld()
    local ReturnCamera = World:GetPlayerCamera()

    if not ReturnCamera then
        print("[GameCinematics] Main camera not found")
        return false
    end

    -- CineCameraActor는 최초 한 번만 생성하고 재사용한다.
    if not Sequence.CineActor then
        local CineClass = UClass.Find("ACineCameraActor")

        if not CineClass then
            print("[GameCinematics] ACineCameraActor class not found")
            return false
        end

        local Spawned = World:SpawnActor(
            CineClass,
            "GameCinematicCamera"
        )

        if not Spawned then
            print("[GameCinematics] Failed to spawn camera")
            return false
        end

        Sequence.CineActor = ACineCameraActor.Cast(Spawned)

        if not Sequence.CineActor then
            print("[GameCinematics] Camera cast failed")
            return false
        end
    end

    local Cine = Sequence.CineActor
    local Camera = Cine:GetCameraComponent()

    -- 현재 카메라의 구도를 시작점으로 사용한다.
    local StartLocation = ReturnCamera:GetWorldLocation()
    local StartRotation = ReturnCamera:GetWorldRotation()

    -- 현재 카메라의 방향을 기준으로 이동 목표를 계산한다.
    local Orientation = StartRotation:Quaternion()
    local Forward = Orientation:GetForwardVector()
    local Right = Orientation:GetRightVector()
    local Up = Orientation:GetUpVector()

    local EndLocation =
        StartLocation
        + Right * 10.0
        + Up * 4.0
        - Forward * 8.0

    local EndRotation = FRotator.new(
        StartRotation.Pitch - 8.0,
        StartRotation.Yaw + 25.0,
        StartRotation.Roll
    )

    Cine:SetSensorWidth(36.0)
    Cine:SetFocalLength(50.0)

    Camera:SetWorldLocation(StartLocation, false)
    Camera:SetWorldRotation(StartRotation, false)

    -- 시작할 때 부드럽게 CineCamera로 전환한다.
    if not World:SetMainCameraComponent(Camera, 0.35) then
        print("[GameCinematics] Failed to set camera")
        return false
    end

    if not World:BeginSlomo(0.35, 0.0, ETimeMode.RealTime) then
        -- 배율 변경에 실패하면 원래 카메라로 복귀
        World:SetMainCameraComponent(ReturnCamera, 0.0)

        print("[GameCinematics] Failed to set time dilation")
        return false
    end

    Sequence.StartLocation = StartLocation
    Sequence.EndLocation = EndLocation
    Sequence.StartRotation = StartRotation
    Sequence.EndRotation = EndRotation

    Sequence.StartFocalLength = 50.0
    Sequence.EndFocalLength = 85.0

    Sequence.Elapsed = 0.0
    Sequence.Duration = math.max(Duration or 3.0, 0.1)
    Sequence.ReturnCamera = ReturnCamera
    Sequence.bPlaying = true

    ScriptComponent:SetComponentTickEnabled(true)

    print("[GameCinematics] Camera sequence started")
    return true
end

function TickComponent(Self, DeltaSeconds)
    if not Sequence.bPlaying then
        return
    end

    Sequence.Elapsed = Sequence.Elapsed + DeltaSeconds

    local Alpha = math.min(
        Sequence.Elapsed / Sequence.Duration,
        1.0
    )

    -- SmoothStep: 시작과 끝에서 움직임을 부드럽게 만든다.
    local SmoothAlpha = Alpha * Alpha * (3.0 - 2.0 * Alpha)

    local Cine = Sequence.CineActor
    local Camera = Cine:GetCameraComponent()

    -- 위치 보간
    local Location = Sequence.StartLocation
        + (Sequence.EndLocation - Sequence.StartLocation)
        * SmoothAlpha

    Camera:SetWorldLocation(Location, false)

    -- 회전 보간
    local Start = Sequence.StartRotation
    local Finish = Sequence.EndRotation

    local Rotation = FRotator.new(
        Start.Pitch + (Finish.Pitch - Start.Pitch) * SmoothAlpha,
        Start.Yaw + (Finish.Yaw - Start.Yaw) * SmoothAlpha,
        Start.Roll + (Finish.Roll - Start.Roll) * SmoothAlpha
    )

    Camera:SetWorldRotation(Rotation, false)

    -- 초점 거리 변화에 따라 FOV도 갱신된다.
    local FocalLength =
        Sequence.StartFocalLength
        + (Sequence.EndFocalLength - Sequence.StartFocalLength)
        * SmoothAlpha

    Cine:SetFocalLength(FocalLength)

    -- 시퀀스 종료 시 기존 카메라로 복귀한다.
    if Alpha >= 1.0 then
        local World = Actor:GetWorld()

        Sequence.bPlaying = false

        World:EndSlomo(0.0, ETimeMode.RealTime)

        if Sequence.ReturnCamera then
            World:SetMainCameraComponent(
                Sequence.ReturnCamera,
                0.5
            )
        end

        Sequence.ReturnCamera = nil
        Self:SetComponentTickEnabled(false)

        print("[GameCinematics] Camera sequence finished")
    end
end
