#include "EnginePCH.h"
#include "MathUtility.h"
#include "Rotator.h"
#include "Math/EngineMath.h"

FRotator::FRotator()
{
	Pitch = 0; Yaw = 0; Roll = 0;
}

FRotator::FRotator(float P, float Y, float R)
{
	Pitch = P; Yaw = Y; Roll = R;
}

FRotator::FRotator(FVector V)
{
	Pitch = V.X; Yaw = V.Y; Roll = V.Z;
}


FQuat FRotator::Quaternion() const
{
	// Degree를 Radian으로 변환
	float PRad = Pitch * DEGREES_TO_RADIANS;
	float YRad = Yaw * DEGREES_TO_RADIANS;
	float RRad = Roll * DEGREES_TO_RADIANS;
	
	return FQuat::MakeFromEuler(RRad, PRad, YRad);
}

FRotator FRotator::Identitiy = FRotator(0, 0, 0);

FRotator operator+(FRotator Rot, const FVector& Vec)
{
	Rot.Roll += Vec.X;
	Rot.Pitch += Vec.Y;
	Rot.Yaw += Vec.Z;

	return Rot;
}