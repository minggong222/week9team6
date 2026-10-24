#pragma once

#include "Math/Vector.h"
#include "Math/Box.h"

class FCollisionGeometry
{
public:
	static bool IntersectsAABB(const FBox& A, const FBox& B);

    static bool IntersectsSphereSphere(const FVector& CenterA, float RadiusA, const FVector& CenterB, float RadiusB);

    static bool IntersectsSphereCapsule(const FVector& SphereCenter, float SphereRadius,  const FVector& CapsuleStart, const FVector& CapsuleEnd, float CapsuleRadius);

    static bool IntersectsSphereBox(const FVector& SphereCenter, float SphereRadius, const FVector& BoxCenter, const FRotator& BoxRotation, const FVector& BoxExtent);

    static bool IntersectsCapsuleCapsule(const FVector& CapsuleAStart, const FVector& CapsuleAEnd, float CapsuleARadius, const FVector& CapsuleBStart, const FVector& CapsuleBEnd, float CapsuleBRadius);

    static bool IntersectsCapsuleBox( const FVector& CapsuleStart, const FVector& CapsuleEnd, float CapsuleRadius, const FVector& BoxCenter, const FRotator& BoxRotation, const FVector& BoxExtent);

    static bool IntersectsBoxBox(const FVector& CenterA, const FRotator& RotationA, const FVector& ExtentA, const FVector& CenterB, const FRotator& RotationB, const FVector& ExtentB);
};