#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "UObject/Class.h"

void LuaRegisterObject(sol::state& State)
{
    State.new_usertype<UObject>("UObject", sol::no_constructor,
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "GetName", &UObject::GetName,
        "GetUUID", &UObject::GetUUID
        // "GetClass", &UObject::GetClass,
        // "IsA", static_cast<bool (UObject::*)(const UClass*)>(&UObject::IsA)
    );
    State.new_usertype<UClass>("UClass", sol::no_constructor,
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "GetName", [](const UClass& Class) { return Class.Name; },
        // "IsChildOf", &UClass::IsChildOf,
        "Find", &FindClass
    );
}
