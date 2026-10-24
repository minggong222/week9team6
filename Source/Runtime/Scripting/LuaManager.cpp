#include "EnginePCH.h"
#include "LuaManager.h"

#include "luajit.h"

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

FLuaInstance* FLuaManager::CreateInstance(FStringView Path, UObject* Owner)
{
    int32 Index = Instances.Num();
    Instances.Add(MakeUnique<FLuaInstance>(State, Index, Owner));

    FLuaInstance* Lua = Instances.Last().get();

    if (!Lua->Load(Path))
    {
        Lua->Destroy();
        Instances.RemoveLast();
        return nullptr;
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

    Instances[Index]->Destroy();
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
