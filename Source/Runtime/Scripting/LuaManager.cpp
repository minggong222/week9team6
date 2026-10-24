#include "EnginePCH.h"
#include "LuaManager.h"

#include "luajit.h"
#include <cmath>

extern void LuaRegisterObject(sol::state& State);
extern void LuaRegisterActor(sol::state& State);
extern void LuaRegisterMath(sol::state& State);
extern void LuaRegisterComponent(sol::state& State);
extern void LuaRegisterInput(sol::state& State);

void LuaRegister(sol::state& State)
{
    // TODO: 모든 기본 함수 붙일 것
    LuaRegisterObject(State);
    LuaRegisterActor(State);
    LuaRegisterMath(State);
    LuaRegisterComponent(State);
    LuaRegisterInput(State);
}

void FLuaManager::Initialize()
{
    State.open_libraries(sol::lib::base);

    // Lua에서 호출할 C++ 함수
    LuaRegister(State);
}

FLuaInstance* FLuaManager::CreateInstance(
    FStringView Path,
    UObject* Owner
)
{
    int32 Index = Instances.Num();
    Instances.Add(MakeUnique<FLuaInstance>(State, Index, Owner));

    FLuaInstance* Instance = Instances.Last().get();
    Instance->OnLoaded.AddDynamic(this, &FLuaManager::OnInstanceLoaded);
    Instance->OnUnloaded.AddDynamic(this, &FLuaManager::OnInstanceUnloaded);
    return Instance;
}

bool FLuaManager::SetJITEnable(bool Value)
{
    lua_State* L = State.lua_state();

    int Mode = Value ? LUAJIT_MODE_ON : LUAJIT_MODE_OFF;

    // JIT의 상태를 변경.
    if (!luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE | Mode))
    {
        return false;
    }

    bJITEnabled = Value;

    if (Value == false)
    {
        // 이미 컴파일된 JIT 캐시를 비우기
        return luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_FLUSH);
    }

    return true;
}

bool FLuaManager::ReloadAll()
{
    bool bSuccessed = true;

    for (auto& Instance : Instances)
    {
        bSuccessed = Instance->Reload() && bSuccessed;
    }

    return bSuccessed;
}

bool FLuaManager::Reload(int32 Index)
{
    if (!CheckRange(Index))
    {
        return false;
    }

    return Instances[Index]->Reload();
}

bool FLuaManager::Remove(int32 Index)
{
    if (!CheckRange(Index))
    {
        return false;
    }

    Instances[Index]->Unload();
    Instances.RemoveAtSwap(Index);

    if (Index >= Instances.Num())
    {
        return true;
    }

    Instances[Index]->Index = Index;

    return true;
}

bool FLuaManager::Remove(FLuaInstance* Instance)
{
    if (!Instance)
    {
        return false;
    }

    return Remove(Instance->Index);
}

bool FLuaManager::CheckRange(int32 Index) const
{
    if (Index < 0)
    {
        return false;
    }

    if (Index >= Instances.Num())
    {
        return false;
    }

    return true;
}

void FLuaManager::OnInstanceLoaded(FLuaInstance& Instance)
{
    Instance.SetGlobalVariable("StartCoroutine",
        [this, Owner = &Instance](sol::protected_function Function)
        {
            StartCoroutine(Owner, std::move(Function));
        });
    Instance.SetGlobalVariable("Wait", sol::yielding([](float Seconds)
        {
            if (!std::isfinite(Seconds) || Seconds < 0.0f)
                throw sol::error("Wait expects a finite, non-negative number of seconds");
            return Seconds;
        }));
}

void FLuaManager::OnInstanceUnloaded(FLuaInstance& Instance)
{
    StopCoroutines(&Instance);
}

void FLuaManager::StopCoroutines(FLuaInstance* Instance)
{
    // Defer removal: a running coroutine can trigger its own EndPlay or unload.
    for (auto& Task : Coroutines)
    {
        if (Task->Instance == Instance)
            Task->bCancelled = true;
    }
}

void FLuaManager::StartCoroutine(FLuaInstance* Instance, sol::protected_function Function)
{
    if (!Instance || !Function.valid())
        return;

    bool bOwned = false;
    for (const auto& Candidate : Instances)
        if (Candidate.get() == Instance) { bOwned = true; break; }
    if (!bOwned)
        return;

    auto Task = MakeUnique<FCoroutineTask>();
    Task->Instance = Instance;
    Task->Thread = sol::thread::create(State.lua_state());
    Task->Coroutine = sol::coroutine(Task->Thread.thread_state(), Function);

    // Stable address even when nested StartCoroutine calls grow the list.
    FCoroutineTask* RunningTask = Task.get();
    Coroutines.Add(std::move(Task));
    if (!ResumeCoroutine(*RunningTask))
    {
        for (int32 Index = 0; Index < Coroutines.Num(); ++Index)
        {
            if (Coroutines[Index].get() == RunningTask)
            {
                Coroutines.RemoveAt(Index);
                break;
            }
        }
    }
}

bool FLuaManager::ResumeCoroutine(FCoroutineTask& Task)
{
    if (Task.bCancelled)
        return false;

    sol::protected_function_result Result = Task.Coroutine();
    if (!Result.valid())
    {
        sol::error Error = Result;
        HTR_LOG(Error, "Coroutine failed: {}", Error.what());
        return false;
    }
    if (Task.bCancelled || !Task.Coroutine.runnable())
        return false;

    Task.RemainingTime = 0.0f;
    if (Result.return_count() > 0 && Result.get_type() == sol::type::number)
    {
        const float Seconds = Result.get<float>();
        if (!std::isfinite(Seconds) || Seconds < 0.0f)
        {
            HTR_LOG(Error, "Coroutine yielded an invalid wait time");
            return false;
        }
        Task.RemainingTime = Seconds;
    }
    return true;
}

void FLuaManager::UpdateCoroutines(float DeltaTime)
{
    if (!std::isfinite(DeltaTime) || DeltaTime < 0.0f)
        return;

    // New tasks started while resuming wait until the next frame's update.
    for (int32 Index = Coroutines.Num() - 1; Index >= 0; --Index)
    {
        FCoroutineTask& Task = *Coroutines[Index];
        Task.RemainingTime -= DeltaTime;
        if (Task.bCancelled || (Task.RemainingTime <= 0.0f && !ResumeCoroutine(Task)))
            Coroutines.RemoveAt(Index);
    }
}
