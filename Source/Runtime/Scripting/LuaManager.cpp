#include "EnginePCH.h"
#include "LuaManager.h"
#include "LuaRegistry.h"

#include "luajit.h"
#include <cmath>

extern void LuaRegisterObject(sol::state& State);
extern void LuaRegisterWorld(sol::state& State);
extern void LuaRegisterActor(sol::state& State);
extern void LuaRegisterMath(sol::state& State);
extern void LuaRegisterComponent(sol::state& State);
extern void LuaRegisterInput(sol::state& State);
extern void LuaRegisterContainer(sol::state& State);

void LuaPrint(FStringView ComponentName, float Seconds, sol::this_state LuaState, sol::variadic_args Args)
{
    sol::state_view State{ LuaState };
    sol::protected_function ToString = State["tostring"];
    FString Message;
    bool bFirst = true;

    // Lua의 tostring을 사용하여 nil, boolean, __tostring도 동일하게 출력합니다.
    for (const auto Argument : Args)
    {
        sol::protected_function_result Result = ToString(Argument);
        if (!Result.valid())
        {
            sol::error Error = Result;
            throw Error;
        }

        const sol::object Text = Result;
        if (Text.get_type() != sol::type::string)
        {
            throw sol::error("print: tostring must return a string");
        }

        if (!bFirst)
        {
            Message += '\t';
        }

        Message += Result.get<FString>();
        bFirst = false;
    }

    HTR_LOG(Info, "[{}] ({:.3f}) {}", ComponentName, Seconds, Message);
}

void LuaRegister(sol::state& State)
{
    // TODO: 모든 기본 함수 붙일 것
    LuaRegisterObject(State);
    LuaRegisterActor(State);
    LuaRegisterMath(State);
    LuaRegisterContainer(State);
    LuaRegisterComponent(State);
    LuaRegisterWorld(State);
    LuaRegisterInput(State);
}

void FLuaManager::Initialize(FLuaRegistry& InRegistry)
{
    Registry = &InRegistry;
    ElapsedSeconds = 0.0f;
    State.open_libraries(sol::lib::base, sol::lib::math);

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
    Instance->OnHotReloaded.AddDynamic(this, &FLuaManager::OnInstanceHotReloaded);
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

    for (int32 Index = 0; Index < Instances.Num(); ++Index)
    {
        bSuccessed = Reload(Index) && bSuccessed;
    }

    return bSuccessed;
}

bool FLuaManager::Reload(int32 Index)
{
    if (!Registry || !CheckRange(Index))
    {
        return false;
    }

    const FLuaScriptSource* Script = Registry->FindOrLoad(Instances[Index]->GetPath());
    return Script && Instances[Index]->Reload(Script->Source);
}

bool FLuaManager::RequestHotReload(FLuaInstance* Instance)
{
    if (!Registry || !Instance)
    {
        return false;
    }

    for (const auto& Candidate : Instances)
    {
        if (Candidate.get() != Instance)
        {
            continue;
        }

        if (!Instance->GetInitialized())
        {
            return false;
        }

        for (FLuaInstance* Pending : PendingHotReloads)
        {
            if (Pending == Instance)
            {
                return true;
            }
        }

        PendingHotReloads.Add(Instance);
        HTR_LOG(Info, "Lua Hot Reload 예약: {}", Instance->GetPath());
        return true;
    }

    return false;
}

void FLuaManager::ProcessPendingHotReloads()
{
    // 콜백에서 추가된 요청은 다음 프레임에 처리합니다.
    TArray<FLuaInstance*> Requests = std::move(PendingHotReloads);
    PendingHotReloads = {};

    for (FLuaInstance* Request : Requests)
    {
        for (const auto& Instance : Instances)
        {
            if (Instance.get() == Request)
            {
                const FString Path = Instance->GetPath();
                FString Source;
                if (Registry && Registry->ReadCandidate(Path, Source) && Instance->HotReload(Source))
                {
                    // 실행까지 성공해야 다음 PIE에서 사용할 소스 버전을 갱신합니다.
                    const FLuaScriptSource& Script = Registry->Accept(Path, std::move(Source));
                    HTR_LOG(Info, "Lua Hot Reload 완료: {} (버전 {})", Path, Script.Version);
                }

                break;
            }
        }
    }
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
    // 인스턴스별로 바인딩하여 일반 함수와 coroutine 모두 소유 컴포넌트를 식별합니다.
    Instance.SetGlobalVariable("print",
        [this, Owner = &Instance](sol::this_state LuaState, sol::variadic_args Args)
        {
            UObject* Component = Owner->GetOwner();
            const FString Name = Component ? Component->GetName() : "Lua";
            LuaPrint(Name, ElapsedSeconds, LuaState, Args);
        });

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

    for (int32 Index = PendingHotReloads.Num() - 1; Index >= 0; --Index)
    {
        if (PendingHotReloads[Index] == &Instance)
        {
            PendingHotReloads.RemoveAt(Index);
        }
    }
}

void FLuaManager::OnInstanceHotReloaded(FLuaInstance& Instance)
{
    // 안전한 교체 시점이므로 다음 업데이트까지 기다리지 않고 참조를 해제합니다.
    for (int32 Index = Coroutines.Num() - 1; Index >= 0; --Index)
    {
        if (Coroutines[Index]->Instance == &Instance)
        {
            Coroutines.RemoveAt(Index);
        }
    }
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

    if (Instance->bHotReloading)
    {
        throw sol::error("Hot Reload 중에는 최상위 코드에서 coroutine을 시작할 수 없습니다.");
    }

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

    ElapsedSeconds += DeltaTime;

    // New tasks started while resuming wait until the next frame's update.
    for (int32 Index = Coroutines.Num() - 1; Index >= 0; --Index)
    {
        FCoroutineTask& Task = *Coroutines[Index];
        Task.RemainingTime -= DeltaTime;
        if (Task.bCancelled || (Task.RemainingTime <= 0.0f && !ResumeCoroutine(Task)))
            Coroutines.RemoveAt(Index);
    }
}
