#include "EnginePCH.h"
#include "WorldTimeManager.h"

#include <algorithm>
#include <cmath>

namespace
{
	bool IsValidSeconds(float Value)
	{
		return std::isfinite(Value) && Value >= 0.0f;
	}

	bool IsValidMode(ETimeMode Mode)
	{
		return Mode == ETimeMode::GameTime || Mode == ETimeMode::RealTime;
	}
}

float FWorldTimeManager::FTimeEffect::Update(float Seconds)
{
	// 단계를 넘어간 여유 시간도 다음 단계에 넘겨, 낮은 FPS에서도 짧은 효과를 완료한다.
	while (IsActive())
	{
		if (Phase == EPhase::Hold && bHoldUntilEnd)
			return 0.0f;

		const float Duration = Phase == EPhase::BlendIn ? BlendIn : Phase == EPhase::Hold ? Hold : BlendOut;
		const float Step = std::min(Seconds, std::max(0.0f, Duration - Elapsed));
		Elapsed += Step;
		Seconds -= Step;

		const float Alpha = Duration > 0.0f ? std::min(Elapsed / Duration, 1.0f) : 1.0f;
		if (Phase == EPhase::BlendIn)
			CurrentScale = StartScale + (TargetScale - StartScale) * Alpha;
		else if (Phase == EPhase::BlendOut)
			CurrentScale = TargetScale + (ReturnScale - TargetScale) * Alpha;

		if (Elapsed < Duration)
			break;

		Elapsed = 0.0f;
		if (Phase == EPhase::BlendIn)
		{
			CurrentScale = TargetScale;
			Phase = EPhase::Hold;
		}
		else if (Phase == EPhase::Hold)
			Phase = EPhase::BlendOut;
		else
		{
			CurrentScale = ReturnScale;
			Phase = EPhase::Inactive;
		}
	}

	return Seconds;
}

void FWorldTimeManager::Tick(float FrameDeltaSeconds, float InRealDeltaSeconds, bool bUpdateEffects, bool bApplyDilation)
{
	RealDeltaSeconds = IsValidSeconds(InRealDeltaSeconds) ? InRealDeltaSeconds : 0.0f;
	FrameDeltaSeconds = IsValidSeconds(FrameDeltaSeconds) ? FrameDeltaSeconds : 0.0f;

	if (bUpdateEffects)
	{
		float RemainingRealSeconds = RealDeltaSeconds;
		if (HitStop.IsActive())
		{
			RemainingRealSeconds = HitStop.Update(RemainingRealSeconds);
			GlobalTimeDilation = HitStop.CurrentScale;
		}

		// Hit Stop이 우선하며, 그동안 Slomo는 멈춘다. 복귀 후 남은 시간만 이어간다.
		if (!HitStop.IsActive() && Slomo.IsActive())
		{
			const float RemainingRatio = RealDeltaSeconds > 0.0f ? RemainingRealSeconds / RealDeltaSeconds : 0.0f;
			const float EffectDelta = Slomo.Mode == ETimeMode::RealTime
				? RemainingRealSeconds : FrameDeltaSeconds * RemainingRatio * GlobalTimeDilation;
			Slomo.Update(EffectDelta);
			GlobalTimeDilation = Slomo.CurrentScale;
		}
	}

	DeltaSeconds = FrameDeltaSeconds * (bApplyDilation ? GlobalTimeDilation : 1.0f);
}

bool FWorldTimeManager::SetGlobalTimeDilation(float Value)
{
	if (!IsValidSeconds(Value))
		return false;

	// 직접 설정은 진행 중인 효과를 대체한다. 이전 효과가 나중에 배율을 덮어쓰지 않는다.
	Slomo = {};
	HitStop = {};
	GlobalTimeDilation = std::clamp(Value, 0.0f, 10.0f);
	return true;
}

bool FWorldTimeManager::StartSlomo(float Scale, float BlendIn, float Hold, float BlendOut, ETimeMode Mode, bool bHoldUntilEnd)
{
	if (!IsValidSeconds(Scale) || !IsValidSeconds(BlendIn) || !IsValidSeconds(Hold)
		|| !IsValidSeconds(BlendOut) || !IsValidMode(Mode))
		return false;

	const float CurrentScale = HitStop.IsActive() ? HitStop.ReturnScale : GlobalTimeDilation;
	const float ReturnScale = Slomo.IsActive() ? Slomo.ReturnScale : CurrentScale;
	Slomo = {};
	Slomo.Phase = EPhase::BlendIn;
	Slomo.Mode = Mode;
	Slomo.StartScale = CurrentScale;
	Slomo.CurrentScale = CurrentScale;
	Slomo.TargetScale = std::clamp(Scale, 0.0f, 10.0f);
	Slomo.ReturnScale = ReturnScale;
	Slomo.BlendIn = BlendIn;
	Slomo.Hold = Hold;
	Slomo.BlendOut = BlendOut;
	Slomo.bHoldUntilEnd = bHoldUntilEnd;
	Slomo.Update(0.0f);

	if (HitStop.IsActive())
		HitStop.ReturnScale = Slomo.CurrentScale;
	else
		GlobalTimeDilation = Slomo.CurrentScale;
	return true;
}

bool FWorldTimeManager::PlaySlomo(float Scale, float BlendIn, float Hold, float BlendOut, ETimeMode Mode)
{
	return StartSlomo(Scale, BlendIn, Hold, BlendOut, Mode, false);
}

bool FWorldTimeManager::BeginSlomo(float Scale, float BlendIn, ETimeMode Mode)
{
	return StartSlomo(Scale, BlendIn, 0.0f, 0.0f, Mode, true);
}

bool FWorldTimeManager::EndSlomo(float BlendOut, ETimeMode Mode)
{
	if (!IsValidSeconds(BlendOut) || !IsValidMode(Mode) || !Slomo.IsActive())
		return false;

	// 진입 보간 도중 종료해도 현재 배율에서 끊김 없이 복귀한다.
	Slomo.TargetScale = Slomo.CurrentScale;
	Slomo.Phase = EPhase::BlendOut;
	Slomo.Mode = Mode;
	Slomo.BlendOut = BlendOut;
	Slomo.Elapsed = 0.0f;
	Slomo.bHoldUntilEnd = false;
	Slomo.Update(0.0f);

	if (HitStop.IsActive())
		HitStop.ReturnScale = Slomo.CurrentScale;
	else
		GlobalTimeDilation = Slomo.CurrentScale;
	return true;
}

bool FWorldTimeManager::PlayHitStop(float BlendIn, float Hold, float BlendOut)
{
	if (!IsValidSeconds(BlendIn) || !IsValidSeconds(Hold) || !IsValidSeconds(BlendOut))
		return false;

	const float ReturnScale = HitStop.IsActive() ? HitStop.ReturnScale : GlobalTimeDilation;
	// 중복 요청은 현재 배율에서 다시 시작하되, 남아 있던 정지 구간은 줄이지 않는다.
	if (HitStop.Phase == EPhase::Hold)
		Hold = std::max(Hold, HitStop.Hold - HitStop.Elapsed);
	else if (HitStop.Phase == EPhase::BlendIn)
		Hold = std::max(Hold, HitStop.Hold);

	HitStop = {};
	HitStop.Phase = EPhase::BlendIn;
	HitStop.StartScale = GlobalTimeDilation;
	HitStop.CurrentScale = GlobalTimeDilation;
	HitStop.TargetScale = 0.0f;
	HitStop.ReturnScale = ReturnScale;
	HitStop.BlendIn = BlendIn;
	HitStop.Hold = Hold;
	HitStop.BlendOut = BlendOut;
	HitStop.Update(0.0f);
	GlobalTimeDilation = HitStop.CurrentScale;
	return true;
}

void FWorldTimeManager::ResetTimeEffects()
{
	Slomo = {};
	HitStop = {};
	GlobalTimeDilation = 1.0f;
}

void FWorldTimeManager::Reset()
{
	ResetTimeEffects();
	DeltaSeconds = 0.0f;
	RealDeltaSeconds = 0.0f;
}
