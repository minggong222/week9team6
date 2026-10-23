#include "EnginePCH.h"
#include "LuaManager.h"

#include "luajit.h"

// 여러개의 Cpp 파일로 분리
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
