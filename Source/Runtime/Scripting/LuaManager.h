#pragma once

#include "Scripting/LuaInstance.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Core/Types.h"
#include "IO/FileWatcher.h"

class ULuaComponent;
class FLuaRegistry;

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

    void Initialize(FLuaRegistry& InRegistry);
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

    // 중복 요청은 합치며, 실제 교체는 월드 Tick 시작 시 수행합니다.
    bool RequestHotReload(FLuaInstance* Instance);
    void ProcessPendingHotReloads();

    // PIE Tick에서 감시 대상을 갱신하고, 변경된 스크립트의 교체를 예약합니다.
    void UpdateAutoHotReload();

    bool Remove(int32 Handle);
    bool Remove(FLuaInstance* Instance);

private:

    bool CheckRange(int32 Index) const;
    bool ResumeCoroutine(FCoroutineTask& Task);
    void OnInstanceLoaded(FLuaInstance& Instance);
    void OnInstanceUnloaded(FLuaInstance& Instance);
    void OnInstanceHotReloaded(FLuaInstance& Instance);

    sol::state State;
    FLuaRegistry* Registry = nullptr;
    bool bJITEnabled = true;

    // 플레이 경과 초입니다. 일시정지 중에는 증가하지 않습니다.
    float ElapsedSeconds = 0.0f;
    
    TArray<TUniquePtr<FLuaInstance>> Instances;
    TArray<FLuaInstance*> PendingHotReloads;

    FFileWatcher ScriptFileWatcher;
    TSet<FString> WatchedScriptPaths;

    // Destroy coroutine references before instances and their shared Lua state.
    TArray<TUniquePtr<FCoroutineTask>> Coroutines;
};
