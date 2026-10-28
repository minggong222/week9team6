#include "EnginePCH.h"
#include "Components/PrimitiveComponent.h"
#include "LuaContainer.h"
#include "GameFramework/Actor.h"

void LuaRegisterContainer(sol::state& State)
{
    State.new_usertype<FDelegateHandle>("FDelegateHandle", sol::no_constructor,
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        // "IsValid", &FDelegateHandle::IsValid,
        "Reset", &FDelegateHandle::Reset
    );

    LuaRegisterDelegate<UPrimitiveComponent*>(State, "TDelegate");

    // TArray는 Lua 값을 담는 배열이며, 타입이 정해진 배열은 별도로 등록합니다.
    // LuaRegisterArray<sol::object>(State, "TArray");
    // LuaRegisterArray<float>(State, "TArray_float");
    // LuaRegisterArray<int32>(State, "TArray_int32");
    // LuaRegisterArray<uint32>(State, "TArray_uint32");
    // LuaRegisterArray<FString>(State, "TArray_string");

    // LuaRegisterArray<UObject*>(State, "TArray_UObject");
    // LuaRegisterArray<AActor*>(State, "TArray_AActor");
    // LuaRegisterArray<UActorComponent*>(State, "TArray_UActorComponent");
    // LuaRegisterArray<USceneComponent*>(State, "TArray_USceneComponent");
    // LuaRegisterArray<UPrimitiveComponent*>(State, "TArray_UPrimitiveComponent");
}
