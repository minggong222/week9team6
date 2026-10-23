#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Math/EngineMath.h"

void LuaRegisterMathQuat(sol::state& State)
{
    State.new_usertype<FQuat>(
        "FQuat",
        sol::constructors<
            FQuat(),
            FQuat(float, float, float),
            FQuat(float, float, float, float),
            FQuat(const FQuat&)
        >(),
        "X", &FQuat::X,
        "Y", &FQuat::Y,
        "Z", &FQuat::Z,
        "W", &FQuat::W,
        "RotateVector", &FQuat::RotateVector,
        "Normalized", &FQuat::Normalized,
        "Inverse", &FQuat::Inverse,
        "ToFRotator", &FQuat::ToFRotator,
        "GetForwardVector", &FQuat::GetForwardVector,
        "GetRightVector", &FQuat::GetRightVector,
        "GetUpVector", &FQuat::GetUpVector,
        "MakeFromAxisAngle", &FQuat::MakeFromAxisAngle,
        sol::meta_function::multiplication, &FQuat::operator*
    );

    State.new_usertype<FRotator>(
        "FRotator",
        sol::constructors<
            FRotator(),
            FRotator(float, float, float),
            FRotator(FVector),
            FRotator(const FRotator&)
        >(),
        "Pitch", &FRotator::Pitch,
        "Yaw", &FRotator::Yaw,
        "Roll", &FRotator::Roll,
        "Quaternion", &FRotator::Quaternion,
        sol::meta_function::addition, &FRotator::operator+,
        sol::meta_function::multiplication, &FRotator::operator*,
        sol::meta_function::index, sol::resolve<float(int32) const>(&FRotator::operator[]),
        sol::meta_function::new_index, [](FRotator& V, int32 Index, float Value) { V[Index] = Value; }
    );
}
