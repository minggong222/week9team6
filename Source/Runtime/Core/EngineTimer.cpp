#include "EnginePCH.h"
#include "EngineTimer.h"

void EngineTimer::Init()
{
	PrevTime = Clock::now();
	DeltaTime = RealDeltaTime = TotalTime = 0.0f;
}

void EngineTimer::Tick()
{
	Clock::time_point Current = Clock::now();
	std::chrono::duration<float> Elapsed = Current - PrevTime;

	RealDeltaTime = Elapsed.count();
	DeltaTime = RealDeltaTime;
	PrevTime = Current;

	if (DeltaTime > 0.1f) DeltaTime = 0.1f;
	TotalTime += DeltaTime;
}
