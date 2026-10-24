#pragma once

#include "Scripting/LuaInstance.h"
#include "Containers/Array.h"
#include "Core/Types.h"

class ULuaComponent;

struct FCoroutineTask
{
    FLuaInstance* Instance = nullptr;
    sol::thread Thread;
    sol::coroutine Coroutine;
    float RemainingTime = 0.0f;
    bool bCancelled = false;
};

class FLuaManager
{
public:
    FLuaManager() = default;
    ~FLuaManager() = default;
    FLuaManager(const FLuaManager&) = delete;
    FLuaManager& operator=(const FLuaManager&) = delete;

    void Initialize();
    FLuaInstance* CreateInstance(
        FStringView Path,
        UObject* Owner
    );

    bool SetJITEnable(bool Value);
    bool GetJITEnable() const { return bJITEnabled; }

    void StartCoroutine(FLuaInstance* Instance, sol::protected_function Function);
    void UpdateCoroutines(float DeltaTime);
    void StopCoroutines(FLuaInstance* Instance);

    bool ReloadAll();
    bool Reload(int32 Handle);
    bool Remove(int32 Handle);
    bool Remove(FLuaInstance* Instance);

private:

    bool CheckRange(int32 Index) const;
    bool ResumeCoroutine(FCoroutineTask& Task);
    void OnInstanceLoaded(FLuaInstance& Instance);
    void OnInstanceUnloaded(FLuaInstance& Instance);

    sol::state State;
    bool bJITEnabled = true;
    
    TArray<TUniquePtr<FLuaInstance>> Instances;
    // Destroy coroutine references before instances and their shared Lua state.
    TArray<TUniquePtr<FCoroutineTask>> Coroutines;
};
