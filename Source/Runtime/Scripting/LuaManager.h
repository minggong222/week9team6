#pragma once

#include "Scripting/LuaInstance.h"
#include "Scripting/ScriptInstance.h"
#include "Containers/Array.h"

class ULuaComponent;

class FLuaManager
{
public:
    FLuaManager() = default;
    ~FLuaManager() = default;
    FLuaManager(const FLuaManager&) = delete;
    FLuaManager& operator=(const FLuaManager&) = delete;

    void Initialize();
    FLuaInstance Load(FStringView Path);

    bool SetJITEnable(bool Value);
    bool GetJITEnable() const { return bJITEnabled; }

    // Component/index integration is declared here, but not implemented yet.
    int32 CreateInstance(const FString& Path, ULuaComponent* OwnerComponent);
    bool Invoke(int32 Index, const FString& FunctionName);
    bool Invoke(int32 Index, const FString& FunctionName, float Argument);
    bool Reload(int32 Index);
    void DestroyInstance(int32 Index);

private:
    FScriptInstance* GetInstance(int32 Index);

    // Destroy instances before their shared state.
    sol::state State;
    bool bJITEnabled = true;
    // Deleted slots stay null; indices are not shifted or reused.
    TArray<TUniquePtr<FScriptInstance>> Instances;
};
