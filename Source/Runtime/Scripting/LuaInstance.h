#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Core/EngineLog.h"
#include "Containers/StringView.h"

#include "sol/sol.hpp"

class FLuaInstance
{
private: 

	// 스크립트의 경로를 가리킵니다.
	FString Path;

	sol::state& State;
	sol::environment Environment;

	// 스크립트를 소유하는 객체를 가리킵니다.
	UObject* Owner;

public:

	FLuaInstance(sol::state& InState);

	// 이동 생성자 빼고 모두 삭제.
	// 이동 대입 연산자의 경우, 참조 변수 때문에 컴파일러가 생성할 수 없음. 근데 어처피 쓸 일 없어보임
	FLuaInstance(const FLuaInstance&) = delete;
	FLuaInstance& operator=(const FLuaInstance&) = delete;
	FLuaInstance(FLuaInstance&&) = default;
	FLuaInstance& operator=(FLuaInstance&&) = delete;

	bool Load(FStringView InPath);
	const FString& GetPath() const { return Path; }

	template<typename... Args>
	bool Invoke(FStringView FunctionName, Args... args);

	bool Reload();

	void Destroy();

	UObject* GetOwner() const { return Owner; }
	void SetOwner(UObject* InOwner);

};

template<typename... Args>
bool FLuaInstance::Invoke(FStringView FunctionName, Args... args)
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
