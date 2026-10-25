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
	sol::table EngineBindings;
	bool bHotReloading = false;

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
	
	// 디스크를 읽지 않고 Registry에서 채택한 소스를 새 환경에 실행합니다.
	bool Load(FStringView InPath, FStringView Source);

	// Lua 스크립트에서 함수를 호출합니다.
	template<typename... Args>
	bool Invoke(FStringView FunctionName, Args... args);

	template<typename T>
	bool SetGlobalVariable(FStringView VariableName, T Value);

	// 같은 경로에 전달받은 소스로 실행 상태를 초기화합니다.
	bool Reload(FStringView Source);

	// Lua 호출이 끝난 시점에 실행합니다. 일반 호출자는 매니저에 요청을 예약합니다.
	// 최상위 코드는 선언과 초기화만 수행해야 하며, 테이블 내부 함수는 지원하지 않습니다.
	// 실패 시 기존 환경을 유지하고, 성공 시 OnHotReloaded에서 외부 함수 참조를 갱신합니다.
	bool HotReload(FStringView Source);

	// 로드된 Lua 스크립트를 언로드합니다.
	void Unload();

	UObject* GetOwner() const { return Owner; }
	void SetOwner(UObject* InOwner);

	bool GetInitialized() const { return bInitialized; }

	// 스크립트가 로드될 때 호출될 콜백 함수입니다.
	DECLARE_DELEGATE(OnLoaded, FLuaInstance&);
	DECLARE_DELEGATE(OnUnloaded, FLuaInstance&);
	DECLARE_DELEGATE(OnHotReloaded, FLuaInstance&);

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
