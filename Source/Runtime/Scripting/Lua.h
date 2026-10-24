#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Core/EngineLog.h"
#include "Containers/StringView.h"

#include "sol/sol.hpp"

class FLua
{
private: 

	// 스크립트의 경로를 가리킵니다.
	FString Path;

	// 비소유 참조: 이 스크립트보다 FLuaManager가 오래 살아 있어야 합니다.
	sol::state* State;
	sol::environment Environment;

public:

	FLua(sol::state& InState);

	FLua(const FLua&) = delete;
	FLua& operator=(const FLua&) = delete;
	FLua(FLua&&) = default;
	FLua& operator=(FLua&&) = default;

	bool Load(FStringView InPath);
	const FString& GetPath() const { return Path; }

	template<typename... Args>
	bool Invoke(FStringView FunctionName, Args... args);

	bool Reload();

	void Destroy();

};

template<typename... Args>
bool FLua::Invoke(FStringView FunctionName, Args... args)
{
	// Environment가 없음
	if (!Environment.valid())
	{
		return false;
	}

	sol::protected_function Function = Environment[FunctionName];

	// 함수가 없음
	if (!Function.valid())
	{
		return false;
	}

	sol::protected_function_result Result = Function(args...);

	if (!Result.valid())
	{
		sol::error Error = Result;
		HTR_LOG(Error, "Lua 함수 실행 실패: {}", Error.what());
		return false;
	}

	return true;
}
