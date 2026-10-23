#include "EnginePCH.h"
#include "Lua.h"

#include "Core/EngineLog.h"

FLua::FLua(sol::state& InState)
    : State{ InState }
    , Environment{
        sol::environment{
            State,
            sol::create,
            State.globals()
        }
    }
{}

bool FLua::Load(FStringView InPath)
{
	Path = InPath;

    // Lua 파일을 읽고 최상위 코드를 실행합니다.
    sol::protected_function_result Result = State.safe_script_file(
        InPath.data(),
        Environment,
        sol::script_pass_on_error
    );

    if (!Result.valid()) {
        sol::error Error = Result;
        HTR_LOG(Error, "Lua 스크립트 로드 실패: {}", Error.what());
        return false;
    }

    return true;
}

bool FLua::Reload()
{
    if (!Load(Path))
    {
        return false;
    }

    Environment = sol::environment{
        State,
        sol::create,
        State.globals()
    };

    return true;
}

void FLua::Destroy()
{
    Environment = sol::lua_nil;
}
