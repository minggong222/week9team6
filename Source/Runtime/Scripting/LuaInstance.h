#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Core/EngineLog.h"
#include "Core/Delegates/Delegate.h"
#include "Containers/StringView.h"

#include "sol/sol.hpp"

class FLuaInstance
{
private: 

	// Reload에 사용할 Assets 기준 스크립트 상대 경로입니다.
	FString Path;

	sol::state& State;
	sol::environment Environment;

	// 스크립트를 소유하는 객체를 가리킵니다.
	UObject* Owner;

	// FLuaManager에서의 Index 위치를 가리킵니다.
	int32 Index;
	friend class FLuaManager;

	bool bInitialized = false;

public:

	FLuaInstance(
		sol::state& InState,
		int32 InIndex,
		UObject* InOwner = nullptr
	);

	// 이동 생성자 빼고 모두 삭제.
	// 이동 대입 연산자의 경우, 참조 변수 때문에 컴파일러가 생성할 수 없음. 근데 어처피 쓸 일 없어보임
	FLuaInstance(const FLuaInstance&) = delete;
	FLuaInstance& operator=(const FLuaInstance&) = delete;
	FLuaInstance(FLuaInstance&&) = default;
	FLuaInstance& operator=(FLuaInstance&&) = delete;

	const FString& GetPath() const { return Path; }
	
	// Assets 기준 상대 경로를 받아 내부에서 절대 경로로 변환해 불러옵니다.
	bool Load(FStringView InPath);

	// Lua 스크립트에서 함수를 호출합니다.
	template<typename... Args>
	bool Invoke(FStringView FunctionName, Args... args);

	template<typename T>
	bool SetGlobalVariable(FStringView VariableName, T Value);

	// Path에서 Lua 스크립트를 다시 불러옵니다.
	bool Reload();

	// 로드된 Lua 스크립트를 언로드합니다.
	void Unload();

	UObject* GetOwner() const { return Owner; }
	void SetOwner(UObject* InOwner);

	bool GetInitialized() const { return bInitialized; }

	// 스크립트가 로드될 때 호출될 콜백 함수입니다.
	DECLARE_DELEGATE(OnLoaded, FLuaInstance&);
	DECLARE_DELEGATE(OnUnloaded, FLuaInstance&);

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

template<typename T>
inline bool FLuaInstance::SetGlobalVariable(FStringView VariableName, T Value)
{
	// Environment가 없음
	if (!Environment.valid())
	{
		return false;
	}

	Environment[VariableName] = Value;

	return true;
}
