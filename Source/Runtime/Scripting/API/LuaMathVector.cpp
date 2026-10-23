#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Math/EngineMath.h"

void LuaRegisterMathVector(sol::state& State)
{
    State.new_usertype<FVector>(
        "FVector",
        sol::constructors<
            FVector(),
            FVector(float),
            FVector(float, float, float),
            FVector(const FVector&)
        >(),
        "X", &FVector::X,
        "Y", &FVector::Y,
        "Z", &FVector::Z,
        "Size", &FVector::Size,
        "SizeSquared", &FVector::SizeSquared,
        "Length", &FVector::Length,
        "Set", &FVector::Set,
        "Normalized", &FVector::Normalized,
        "GetSafeNormal", sol::overload(
            [](const FVector& V) { return V.GetSafeNormal(); },
            [](const FVector& V, float Tolerance) { return V.GetSafeNormal(Tolerance); },
            &FVector::GetSafeNormal
        ),
        "Dot", sol::resolve<float(const FVector&) const>(&FVector::Dot),
        "Cross", sol::resolve<FVector(const FVector&) const>(&FVector::Cross),
        "Distance", &FVector::Distance,
        "GetAbs", &FVector::GetAbs,
        "GetClampedToMaxSize", &FVector::GetClampedToMaxSize,

        sol::meta_function::unary_minus, sol::resolve<FVector() const>(&FVector::operator-),
        sol::meta_function::subtraction, sol::resolve<FVector(const FVector&) const>(&FVector::operator-),
        sol::meta_function::addition, &FVector::operator+,
        sol::meta_function::multiplication, sol::overload(
            sol::resolve<FVector(const FVector&) const>(&FVector::operator*),
            sol::resolve<FVector(const float&) const>(&FVector::operator*)
        ),
        sol::meta_function::division, sol::overload(
            sol::resolve<FVector(const FVector&) const>(&FVector::operator/),
            sol::resolve<FVector(const float&) const>(&FVector::operator/)
        ),
        sol::meta_function::power_of, &FVector::operator^,

        // 비교 함수를 동일한 비교 함수 객체로 만들기.
        // 함수 객체가 다르다는 이유로 false가 반환되지 않도록 함
        sol::meta_function::equal_to, sol::make_object(State, &FVector::operator==),
        sol::meta_function::index, sol::resolve<float(int32) const>(&FVector::operator[]),
        sol::meta_function::new_index, [](FVector& V, int32 Index, float Value) { V[Index] = Value; }
    );

    State.new_usertype<FVector2>(
        "FVector2",
        sol::constructors<
            FVector2(),
            FVector2(float),
            FVector2(float, float),
            FVector2(const FVector2&)
        >(),
        "X", &FVector2::X,
        "Y", &FVector2::Y,
        sol::meta_function::addition, sol::resolve<FVector2(const FVector2&) const>(&FVector2::operator+),
        sol::meta_function::subtraction, sol::resolve<FVector2(const FVector2&) const>(&FVector2::operator-),
        sol::meta_function::multiplication, sol::resolve<FVector2(float) const>(&FVector2::operator*),
        sol::meta_function::index, sol::resolve<const float&(uint64) const>(&FVector2::operator[]),
        sol::meta_function::new_index, [](FVector2& V, uint64 Index, float Value) { V[Index] = Value; }
    );

    State.new_usertype<FVector4>(
        "FVector4",
        sol::constructors<
            FVector4(),
            FVector4(float),
            FVector4(float, float, float, float),
            FVector4(const FVector&),
            FVector4(const FVector&, float),
            FVector4(const FVector4&)
        >(),
        "X", &FVector4::X,
        "Y", &FVector4::Y,
        "Z", &FVector4::Z,
        "W", &FVector4::W,
        "Size", &FVector4::Size,
        "Length", &FVector4::Length,
        "Set", sol::overload(
            sol::resolve<void(float, float, float, float)>(&FVector4::Set),
            sol::resolve<void(const FVector&, float)>(&FVector4::Set),
            sol::resolve<void(const FVector4&)>(&FVector4::Set),
            [](FVector4& V, const FVector& Other) { V.Set(Other); }
        ),
        "Normalized", &FVector4::Normalized,
        "Dot", &FVector4::Dot,
        "Cross", &FVector4::Cross,
        "Distance", &FVector4::Distance,
        "GetAbs", &FVector4::GetAbs,
        sol::meta_function::unary_minus, sol::resolve<FVector4()>(&FVector4::operator-),
        sol::meta_function::addition, &FVector4::operator+,
        sol::meta_function::subtraction, sol::resolve<FVector4(const FVector4&) const>(&FVector4::operator-),
        sol::meta_function::multiplication, sol::overload(
            sol::resolve<FVector4(const FVector4&) const>(&FVector4::operator*),
            sol::resolve<FVector4(const float&) const>(&FVector4::operator*),
            sol::resolve<FVector4(const FMatrix&) const>(&FVector4::operator*)
        ),
        sol::meta_function::division, sol::overload(
            sol::resolve<FVector4(const FVector4&) const>(&FVector4::operator/),
            sol::resolve<FVector4(const float&) const>(&FVector4::operator/)
        ),
        sol::meta_function::power_of, &FVector4::operator^,
        sol::meta_function::equal_to, sol::make_object(State, &FVector4::operator==),
        sol::meta_function::index, sol::resolve<float(int32) const>(&FVector4::operator[]),
        sol::meta_function::new_index, [](FVector4& V, int32 Index, float Value) { V[Index] = Value; }
    );
}
