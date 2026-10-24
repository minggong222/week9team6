#include "EnginePCH.h"
#include "CollisionGeometry.h"

bool FCollisionGeometry::IntersectsAABB(const FBox& A, const FBox& B)
{
    return A.Min.X <= B.Max.X && A.Max.X >= B.Min.X &&
        A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y &&
        A.Min.Z <= B.Max.Z && A.Max.Z >= B.Min.Z;
}

bool FCollisionGeometry::IntersectsSphereSphere(const FVector& CenterA, float RadiusA, const FVector& CenterB, float RadiusB)
{
    const FVector Delta = CenterB - CenterA;
    const float RadiusSum = RadiusA + RadiusB;

    return Delta.SizeSquared() <= RadiusSum * RadiusSum;
}