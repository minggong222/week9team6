#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Math/EngineMath.h"

void LuaRegisterMathMatrix(sol::state& State)
{
    State.new_usertype<FMatrix>(
        "FMatrix",
        sol::constructors<
            FMatrix(),
            FMatrix(const FMatrix&),
            FMatrix(const FVector4&, const FVector4&, const FVector4&, const FVector4&),
            FMatrix(
                float, float, float, float,
                float, float, float, float,
                float, float, float, float,
                float, float, float, float
            )
        >(),
        "Identity", sol::property([] { return FMatrix::Identity; }),
        "SetIdentity", &FMatrix::SetIdentity,
        "SetAxes", &FMatrix::SetAxes,
        "SetAxis", &FMatrix::SetAxis,
        "GetColumn", &FMatrix::GetColumn,
        "SetColumn", &FMatrix::SetColumn,
        "GetOrigin", &FMatrix::GetOrigin,
        "SetOrigin", &FMatrix::SetOrigin,
        "GetScaledAxis", [](const FMatrix& M) {
            FVector4 X, Y, Z;
            M.GetScaledAxis(X, Y, Z);
            return std::make_tuple(X, Y, Z);
        },
        "GetUnitAxis", [](const FMatrix& M) {
            FVector4 X, Y, Z;
            M.GetUnitAxis(X, Y, Z);
            return std::make_tuple(X, Y, Z);
        },
        "TransformFVector4", &FMatrix::TransformFVector4,
        "TransformFVector4NoScale", &FMatrix::TransformFVector4NoScale,
        "TransformPosition", &FMatrix::TransformPosition,
        "TransformVector", &FMatrix::TransformVector,
        "TransformVectorNoScale", &FMatrix::TransformVectorNoScale,
        "InverseTransformPosition", &FMatrix::InverseTransformPosition,
        "ApplyScale", &FMatrix::ApplyScale,
        "GetTransposed", &FMatrix::GetTransposed,
        "Inverse", &FMatrix::Inverse,
        "Multiply", &FMatrix::Multiply,
        "Determinant", &FMatrix::Determinant,
        "MakeTranslation", &FMatrix::MakeTranslation,
        "ToRotator", &MatrixToRotator,
        "LookAt", &LookAt,
        "Perspective", &Perspective,
        "Ortho", sol::overload(
            sol::resolve<FMatrix(float, float, float, float)>(&Ortho),
            sol::resolve<FMatrix(float, float, float, float, float, float)>(&Ortho)
        ),
        "GetElement", &FMatrix::GetElement,
        "SetElement", &FMatrix::SetElement,
        sol::meta_function::unary_minus, sol::resolve<FMatrix()>(&FMatrix::operator-),
        sol::meta_function::addition, &FMatrix::operator+,
        sol::meta_function::subtraction, sol::resolve<FMatrix(const FMatrix&) const>(&FMatrix::operator-),
        sol::meta_function::multiplication, sol::overload(
            sol::resolve<FMatrix(const FMatrix&) const>(&FMatrix::operator*),
            sol::resolve<FMatrix(const float&) const>(&FMatrix::operator*),
            sol::resolve<FVector4(const FVector4&) const>(&FMatrix::operator*)
        ),
        sol::meta_function::equal_to, sol::make_object(State, &FMatrix::operator==)
    );
}
