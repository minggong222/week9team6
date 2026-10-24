#include "EnginePCH.h"
#include "Lua.h"

#include "Core/EngineLog.h"

FLua::FLua(sol::state& InState)
    : State{ &InState }
{}

bool FLua::Load(FStringView InPath)
{
    sol::environment NewEnvironment = sol::environment{
        *State,
        sol::create,
        State->globals()
    };

    // Lua 파일을 읽고 최상위 코드를 실행합니다.
    sol::protected_function_result Result = State->safe_script_file(
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

bool FLua::Reload()
{
    if (!Load(Path))
    {
        return false;
    }

    return true;
}

void FLua::Destroy()
{
    Environment = sol::lua_nil;

// 여러개의 Cpp 파일로 분리
extern void LuaRegisterObject(sol::state& State);
extern void LuaRegisterActor(sol::state& State);
extern void LuaRegisterMath(sol::state& State);
extern void LuaRegisterComponent(sol::state& State);
extern void LuaRegisterInput(sol::state& State);

void FLua::RegisterAPI()
{
    LuaRegisterObject(State);
    LuaRegisterActor(State);
    LuaRegisterMath(State);
    LuaRegisterComponent(State);
    LuaRegisterInput(State);
}
