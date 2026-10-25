#include "EnginePCH.h"
#include "LuaInstance.h"

#include "Core/EngineLog.h"
#include "IO/FileUtils.h"

FLuaInstance::FLuaInstance(
    sol::state& InState,
    int32 InIndex,
    UObject* InOwner
)
    : State{ InState }
    , Index{ InIndex }
    , Owner{ InOwner }
{}

bool FLuaInstance::Load(FStringView InPath, FStringView Source)
{
    const FString AssetPath{ InPath };
    Unload();
    const FString FullPath = FileUtils::GetAssetFullPath(AssetPath);

    Environment = sol::environment{
        State,
        sol::create,
        State.globals()
    };

    // 로드 이벤트 호출
    OnLoaded.Broadcast(*this);

    // 엔진 바인딩은 스크립트 함수와 구분하여 다음 환경에도 등록합니다.
    EngineBindings = State.create_table();
    for (const auto& Entry : Environment)
    {
        EngineBindings[Entry.first] = Entry.second;
    }

    // Lua 스크립트를 실행합니다.
    sol::protected_function_result Result = State.safe_script(
        Source,
        Environment,
        sol::script_pass_on_error,
        "@" + FullPath
    );

    if (!Result.valid()) {
        sol::error Error = Result;
        HTR_LOG(Error, "Lua 스크립트 로드 실패: {}", Error.what());
        Unload();
        return false;
    }

    Path = AssetPath;
    bInitialized = true;

    return true;
}

bool FLuaInstance::Reload(FStringView Source)
{
    return Load(Path, Source);
}

bool FLuaInstance::HotReload(FStringView Source)
{
    if (!bInitialized || bHotReloading)
    {
        return false;
    }

    sol::environment NewEnvironment{ State, sol::create, State.globals() };
    for (const auto& Entry : EngineBindings)
    {
        NewEnvironment[Entry.first] = Entry.second;
    }

    // 기존 상태를 공유하지 않고 평가해야 실행 오류가 기존 테이블을 훼손하지 않습니다.
    const FString FullPath = FileUtils::GetAssetFullPath(Path);
    bHotReloading = true;
    sol::protected_function_result Result = State.safe_script(
        Source, NewEnvironment, sol::script_pass_on_error, "@" + FullPath);
    bHotReloading = false;

    if (!Result.valid())
    {
        sol::error Error = Result;
        HTR_LOG(Error, "Lua Hot Reload 실패: {}", Error.what());
        return false;
    }

    for (const auto& Entry : Environment)
    {
        const sol::object& Value = Entry.second;
        const sol::object NewValue = NewEnvironment.raw_get<sol::object>(Entry.first);
        const sol::object Binding = EngineBindings.raw_get<sol::object>(Entry.first);

        // 함수와 coroutine은 이관하지 않습니다. 테이블은 병합 없이 기존 객체를 유지합니다.
        if (Binding != sol::lua_nil || Value.get_type() == sol::type::function
            || Value.get_type() == sol::type::thread || NewValue.get_type() == sol::type::function)
        {
            continue;
        }

        NewEnvironment[Entry.first] = Value;
    }

    Environment = std::move(NewEnvironment);
    OnHotReloaded.Broadcast(*this);
    return true;
}

void FLuaInstance::Unload()
{
    OnUnloaded.Broadcast(*this);
    Environment = sol::lua_nil;
    EngineBindings = sol::lua_nil;
    bInitialized = false;
}

void FLuaInstance::SetOwner(UObject* InOwner)
{
    Owner = InOwner;
}
