#pragma once

#include "Scripting/Lua.h"

#include "sol/sol.hpp"

class FLuaManager
{
private:

	sol::state State;
	bool bJITEnabled = true;

public:

	void Initialize();

	// 새로운 Lua 스크립트를 불러와서 FLua를 반환합니다.
	FLua Load(FStringView Path);

	bool SetJITEnable(bool Value);
	bool GetJITEnable() { return bJITEnabled; }
	bool bJITEnabled = true;

	// Shared execution state; instances are destroyed before this state.
	sol::state State;
	// Deleted slots remain null. Indices are not shifted or reused.
	TArray<TUniquePtr<FScriptInstance>> Instances;

	FScriptInstance* GetInstance(int32 Index);

public:
	FLuaManager() = default;
	~FLuaManager() = default;
	FLuaManager(const FLuaManager&) = delete;
	FLuaManager& operator=(const FLuaManager&) = delete;

	// Runtime implementations will be connected in the next step.
	int32 CreateInstance(const FString& Path, ULuaComponent* OwnerComponent);
	bool Invoke(int32 Index, const FString& FunctionName);
	bool Invoke(int32 Index, const FString& FunctionName, float Argument);
	bool Reload(int32 Index);
	void DestroyInstance(int32 Index);

	// 새로운 Lua 스크립트를 불러와서 FLua를 반환합니다.
	FLua LoadNewScript();

	void SetJITEnable(bool Value);
	bool GetJITEnable();
};