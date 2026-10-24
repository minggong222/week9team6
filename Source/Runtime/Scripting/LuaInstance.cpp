#include "EnginePCH.h"
#include "LuaInstance.h"

#include "Core/EngineLog.h"

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
    sol::environment NewEnvironment = sol::environment{
        State,
        sol::create,
        State.globals()
    };

    // Lua 파일을 읽고 최상위 코드를 실행합니다.
    sol::protected_function_result Result = State.safe_script_file(
        InPath.data(),
        NewEnvironment,
        sol::script_pass_on_error
    );

    if (!Result.valid()) {
        sol::error Error = Result;
        HTR_LOG(Error, "Lua 스크립트 로드 실패: {}", Error.what());
        return false;
    }

    Path = InPath;
    Environment = NewEnvironment;

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

void FLuaInstance::Destroy()
{
    Environment = sol::lua_nil;
}

void FLuaInstance::SetOwner(UObject* InOwner)
{
    Owner = InOwner;
}