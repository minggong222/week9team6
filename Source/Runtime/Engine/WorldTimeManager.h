#pragma once

enum class ETimeMode
{
	// 배율이 0이면 진행하지 않는다. 정지 상태에서 복귀하려면 RealTime을 사용한다.
	GameTime,
	RealTime
};

// 월드의 시간과 시간 효과를 소유한다. 모든 시간 인자는 초 단위다.
class FWorldTimeManager
{
public:
	// 게임 업데이트용 프레임 시간과 제한하지 않은 실제 경과 시간을 별도로 받는다.
	void Tick(float FrameDeltaSeconds, float InRealDeltaSeconds, bool bUpdateEffects = true, bool bApplyDilation = true);
	void Reset();
	void ResetTimeEffects();

	float GetGlobalTimeDilation() const { return GlobalTimeDilation; }
	bool SetGlobalTimeDilation(float Value);
	float GetDeltaSeconds() const { return DeltaSeconds; }
	float GetRealDeltaSeconds() const { return RealDeltaSeconds; }

	// Slomo는 시작 전 배율로 복귀한다. 새 요청은 기존 Slomo를 현재 배율에서 대체한다.
	bool PlaySlomo(float Scale, float BlendIn, float Hold, float BlendOut, ETimeMode Mode);
	bool BeginSlomo(float Scale, float BlendIn, ETimeMode Mode);
	bool EndSlomo(float BlendOut, ETimeMode Mode);
	// 실제 시간으로 진입/정지/복귀하며, 진행 중인 Slomo보다 우선한다.
	bool PlayHitStop(float BlendIn, float Hold, float BlendOut);

private:
	enum class EPhase { Inactive, BlendIn, Hold, BlendOut };

	struct FTimeEffect
	{
		EPhase Phase = EPhase::Inactive;
		ETimeMode Mode = ETimeMode::RealTime;
		float StartScale = 1.0f;
		float TargetScale = 1.0f;
		float ReturnScale = 1.0f;
		float CurrentScale = 1.0f;
		float BlendIn = 0.0f;
		float Hold = 0.0f;
		float BlendOut = 0.0f;
		float Elapsed = 0.0f;
		bool bHoldUntilEnd = false;

		bool IsActive() const { return Phase != EPhase::Inactive; }
		float Update(float Seconds);
	};

	bool StartSlomo(float Scale, float BlendIn, float Hold, float BlendOut, ETimeMode Mode, bool bHoldUntilEnd);

	float GlobalTimeDilation = 1.0f;
	float DeltaSeconds = 0.0f;
	float RealDeltaSeconds = 0.0f;
	FTimeEffect Slomo;
	FTimeEffect HitStop;
};
