
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
    Audio.PlaySFX("Audio/Collision.wav")
end

function StopCinematics()
    if Sequence.bPlaying then
        local World = Actor:GetWorld()

        Sequence.bPlaying = false

        if World then
            World:SetTimeDilation(
                Sequence.PreviousTimeDilation or 1.0
            )

            if Sequence.ReturnCamera then
                World:SetMainCameraComponent(
                    Sequence.ReturnCamera,
                    0.5
                )
            end
        end

        Sequence.PreviousTimeDilation = nil
        Sequence.ReturnCamera = nil

        if ScriptComponent then
            ScriptComponent:SetComponentTickEnabled(false)
        end
    end

    Audio.StopBGM()
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

    Sequence.PreviousTimeDilation = World:GetTimeDilation()

    if not World:SetTimeDilation(0.35) then
        -- 배율 변경에 실패하면 원래 카메라로 복귀
        World:SetMainCameraComponent(ReturnCamera, 0.0)

        Sequence.PreviousTimeDilation = nil

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

        World:SetTimeDilation(
            Sequence.PreviousTimeDilation or 1.0
        )

        Sequence.PreviousTimeDilation = nil

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
