#pragma once
#include "MathUtility.h"
#include <cmath>
#include "Vector.h"
#include "Vector2.h"
#include "Vector4.h"
//#include "TVector.h"
#include "Matrix.h"
#include "Quat.h"
#include "Rotator.h"
#include "Collision/Ray.h"
#include "VectorRegister.h"
#include "MatrixRegister.h"

namespace FMath
{
	template <typename T>
	static inline T Min(const T& A, const T& B)
	{
		return (A >= B ? B : A);
	}

	template <>
	static inline FVector Min(const FVector& A, const FVector& B)
	{
		return FVector(Min(A.X, B.X), Min(A.Y, B.Y), Min(A.Z, B.Z));
	}

	template <typename T>
	static inline T Max(const T& A, const T& B)
	{
		return (A >= B ? A: B);
	}

	template <>
	static inline FVector Max(const FVector& A, const FVector& B)
	{
		return FVector(Max(A.X, B.X), Max(A.Y, B.Y), Max(A.Z, B.Z));
	}

	template <typename T>
	static inline T Clamp(T InValue, T InMin, T InMax)
	{
		return Max(InMin, Min(InValue, InMax));
	}

	template <typename T>
	static inline T Log2(T InValue)
	{
		return static_cast<T>(std::log2(InValue));
	}

	static inline bool IsNearlyZero(float Value, float ErrorTolerance = EPSILON_1E_8)
	{
		return std::abs(Value) <= ErrorTolerance;
	}

	static inline bool IsNearlyEqual(float Value1, float Value2, float ErrorTolerance = EPSILON_1E_4)
	{
		return std::abs(Value1 - Value2) <= ErrorTolerance;
	}

	//static inline float Clamp(float Value, float Min, float Max)
	//{
	//	if (Value < Min) return Min;
	//	if (Value > Max) return Max;
	//	return Value;
	//}

	//static inline uint32 Clamp(uint32 Value, uint32 Min, uint32 Max)
	//{
	//	if (Value < Min) return Min;
	//	if (Value > Max) return Max;
	//	return Value;
	//}


	static inline float RadiansToDegrees(float Radian)
	{
		return Radian * RADIANS_TO_DEGREES;
	}

	static inline float DegreesToRadians(float Degree)
	{
		return Degree * DEGREES_TO_RADIANS;
	}
}
