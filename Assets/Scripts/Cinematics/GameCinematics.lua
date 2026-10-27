-- 게임 연출 진입점. 현재는 BGM과 사망 효과음 재생
function BeginPlay(Self)
    Self:SetComponentTickEnabled(false)
end

function PlayStart()
    if not Audio.PlayBGM("Audio/CrossyBGM.wav") then
        print("[GameManager] Failed to start BGM")
    end
end

function PlayDeath()
    Audio.PlaySFX("Audio/Collision.wav")
end

function StopCinematics()
    Audio.StopBGM()
end
