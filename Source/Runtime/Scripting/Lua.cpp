#include "EnginePCH.h"
#include "Lua.h"

#include "Core/EngineLog.h"

void FLua::Initalize()
{

}

bool FLua::LoadFromFile(FStringView InPath)
{
	Path = InPath;

    State.open_libraries();

    // Lua에서 호출할 C++ 함수
    // TODO: 모든 기본 함수 붙일 것
    RegisterAPI();

    // Lua 파일을 읽고 최상위 코드를 실행합니다.
    auto loaded = State.safe_script_file(InPath.data(), sol::script_pass_on_error);

    if (!loaded.valid()) {
        sol::error error = loaded;
        HTR_LOG(Error, "Lua Script 로드 실패: {}", error.what());
        return false;
    }

    return true;
}


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
