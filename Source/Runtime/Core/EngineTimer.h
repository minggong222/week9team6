#pragma once

#include <chrono>


class EngineTimer
{
public:
	static void Init();
	static void Tick();

	static float GetDeltaTime() { return DeltaTime; };
	static float GetRealDeltaTime() { return RealDeltaTime; }
	static float GetTotalTime() { return TotalTime; }
private:
	using Clock = std::chrono::steady_clock;

	inline static Clock::time_point PrevTime;
	inline static float DeltaTime = 0.0f;
	inline static float RealDeltaTime = 0.0f;
	inline static float TotalTime = 0.0f;
};
