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

bool FLuaInstance::Load(FStringView InPath)
{
    const FString AssetPath{ InPath };
    const FString FullPath = FileUtils::GetAssetFullPath(AssetPath);

    Environment = sol::environment{
        State,
        sol::create,
        State.globals()
    };

    // 로드 이벤트 호출
    OnLoaded.Broadcast(*this);

    // Lua 스크립트를 실행합니다.
    sol::protected_function_result Result = State.safe_script_file(
        FullPath.c_str(),
        Environment,
        sol::script_pass_on_error
    );

    if (!Result.valid()) {
        sol::error Error = Result;
        HTR_LOG(Error, "Lua 스크립트 로드 실패: {}", Error.what());
        bInitialized = false;
        return false;
    }

    Path = AssetPath;
    bInitialized = true;

    return true;
}

bool FLuaInstance::Reload()
{
    if (!Load(Path))
    {
        return false;
    }

    return true;
}

void FLuaInstance::Unload()
{
    Environment = sol::lua_nil;
    bInitialized = false;
}

void FLuaInstance::SetOwner(UObject* InOwner)
{
    Owner = InOwner;
}
