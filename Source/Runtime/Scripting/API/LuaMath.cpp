#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Math/Transform.h"
#include "Math/Color.h"

extern void LuaRegisterMathVector(sol::state& State);
extern void LuaRegisterMathQuat(sol::state& State);
extern void LuaRegisterMathMatrix(sol::state& State);

void LuaRegisterMath(sol::state& State)
{
    LuaRegisterMathVector(State);
    LuaRegisterMathQuat(State);
    LuaRegisterMathMatrix(State);

    State.new_usertype<FTransform>(
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "FTransform",
        sol::constructors<
            FTransform(),
            FTransform(FVector, FVector, FRotator),
            FTransform(const FRotator&, const FVector&),
            FTransform(const FRotator&, const FVector&, const FVector&),
            FTransform(const FTransform&)
        >(),
        "Location", &FTransform::Location,
        "Rotation", &FTransform::Rotation,
        "Scale", &FTransform::Scale
        // "Identity", sol::property([] { return FTransform::Identity; }),
        // "GetLocalMatrix", &FTransform::GetLocalMatrix,
        // "GetForward", &FTransform::GetForward,
        // "GetRight", &FTransform::GetRight,
        // "GetUp", &FTransform::GetUp,
        // "GetOrientation", &FTransform::GetOrientation
    );

    State.new_usertype<FColor>(
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "FColor",
        sol::constructors<
            FColor(),
            FColor(uint32),
            FColor(uint8, uint8, uint8, uint8),
            FColor(const FColor&)
        >(),
        "R", &FColor::R,
        "G", &FColor::G,
        "B", &FColor::B,
        "A", &FColor::A,
        // "ToVector4", &FColor::ToVector4,
        sol::meta_function::equal_to, sol::make_object(State, &FColor::operator==)
    );
}
