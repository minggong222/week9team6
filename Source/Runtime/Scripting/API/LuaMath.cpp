#include "sol/sol.hpp"

#include "Math/Vector.h"
#include "Math/Matrix.h"

void LuaRegisterMath(sol::state& State)
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
        "Normalized", &FVector::Normalized,

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
        sol::meta_function::equal_to, &FVector::operator==,
        sol::meta_function::index, sol::resolve<float(int32) const>(&FVector::operator[]),
        sol::meta_function::new_index, [](FVector& V, int32 Index, float Value) { V[Index] = Value; }
    );

    State.new_usertype<FMatrix>(
        "FMatrix",
        sol::constructors<
            FMatrix(),
            FMatrix(const FVector4&, const FVector4&, const FVector4&, const FVector4&),
            FMatrix(
                float, float, float, float,
                float, float, float, float,
                float, float, float, float,
                float, float, float, float
            )
        >()
    );
}
