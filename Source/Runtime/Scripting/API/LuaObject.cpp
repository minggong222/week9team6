#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "UObject/Class.h"

void LuaRegisterObject(sol::state& State)
{
    State.new_usertype<UObject>("UObject", sol::no_constructor,
        "GetName", &UObject::GetName,
        "GetUUID", &UObject::GetUUID,
        "GetClass", &UObject::GetClass,
        "IsA", static_cast<bool (UObject::*)(const UClass*)>(&UObject::IsA)
    );
    State.new_usertype<UClass>("UClass", sol::no_constructor,
        "GetName", [](const UClass& Class) { return Class.Name; },
        "IsChildOf", &UClass::IsChildOf,
        "Find", &FindClass
    );
}
