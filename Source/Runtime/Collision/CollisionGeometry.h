#pragma once

#include "Math/Vector.h"
#include "Math/Box.h"

class FCollisionGeometry
{
public:
	static bool IntersectsAABB(const FBox& A, const FBox& B);
	static bool IntersectsSphereSphere(const FVector& CenterA, float RadiusA, const FVector& CenterB, float RadiusB);
};