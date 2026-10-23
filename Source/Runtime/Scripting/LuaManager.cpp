#include "EnginePCH.h"
#include "LuaManager.h"

// 여러개의 Cpp 파일로 분리
extern void LuaRegisterObject(sol::state& State);
extern void LuaRegisterActor(sol::state& State);
extern void LuaRegisterMath(sol::state& State);
extern void LuaRegisterComponent(sol::state& State);
extern void LuaRegisterInput(sol::state& State);

void LuaRegister(sol::state& State)
{
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
    // TODO: 모든 기본 함수 붙일 것
    LuaRegister(State);
}


FLua FLuaManager::Load(FStringView Path)
{
	FLua Lua{ State };

    if (!Lua.Load(Path))
    {
        Lua.Destroy();
        throw std::runtime_error("Lua Load Failed");
    }

	return Lua;
}

void FLuaManager::SetJITEnable(bool Value)
{
    bJITEnabled = Value;
}
