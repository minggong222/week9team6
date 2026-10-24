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

bool FCollisionGeometry::IntersectsSphereCapsule(const FVector& SphereCenter, float SphereRadius, const FVector& CapsuleStart, const FVector& CapsuleEnd, float CapsuleRadius)
{
    // 캡슐 중심선의 방향과 길이 제곱
    const FVector Segment = CapsuleEnd - CapsuleStart;
    const float SegmentLengthSquared = Segment.SizeSquared();

    // 구 중심을 캡슐 중심 선분에 투영
    float T = 0.0f;
    if (SegmentLengthSquared > EPSILON_1E_8)
    {
        T = FVector::Dot(SphereCenter - CapsuleStart, Segment) / SegmentLengthSquared;

        T = FMath::Clamp(T, 0.0f, 1.0f);
    }

    // 선분 위에서 구 중심과 가장 가까운 점
    const FVector ClosestPoint = CapsuleStart + Segment * T;
    
    // 가장 가까운 점 사이의 거리와 두 반지름의 합 비교
    const FVector Delta = SphereCenter - ClosestPoint;
    const float RadiusSum = SphereRadius + CapsuleRadius;

    return Delta.SizeSquared() <= RadiusSum * RadiusSum;
}

bool FCollisionGeometry::IntersectsSphereBox(const FVector& SphereCenter, float SphereRadius, const FVector& BoxCenter, const FRotator& BoxRotation, const FVector& BoxExtent)
{
    // 구의 중심을 박스 중심 기준으로 이동
    const FVector RelativeCenter = SphereCenter - BoxCenter;
    // 박스의 회전을 역으로 적용하여 로컬 좌표계로 변환
    const FQuat InverseBoxRotation = BoxRotation.Quaternion().UnitInverse();
    const FVector LocalSphereCenter = InverseBoxRotation.RotateVector(RelativeCenter);

    // 로컬 좌표계에서 박스 내부 가장 가까운 점 계산
    const FVector ClosestPoint(FMath::Clamp(LocalSphereCenter.X, -BoxExtent.X, BoxExtent.X), FMath::Clamp(LocalSphereCenter.Y, -BoxExtent.Y, BoxExtent.Y), FMath::Clamp(LocalSphereCenter.Z, -BoxExtent.Z, BoxExtent.Z));
    
    const FVector Delta = LocalSphereCenter - ClosestPoint;

    return Delta.SizeSquared() <= SphereRadius * SphereRadius;
}

bool FCollisionGeometry::IntersectsCapsuleCapsule(const FVector& CapsuleAStart, const FVector& CapsuleAEnd, float CapsuleARadius, const FVector& CapsuleBStart, const FVector& CapsuleBEnd, float CapsuleBRadius)
{
    // 각 캡슐 중심 선분의 방향벡터
    const FVector D1 = CapsuleAEnd - CapsuleAStart;
    const FVector D2 = CapsuleBEnd - CapsuleBStart;
    const FVector R = CapsuleAStart - CapsuleBStart;
    const float A = D1.SizeSquared();
    const float E = D2.SizeSquared();
    const float RadiusSum = CapsuleARadius + CapsuleBRadius;

    float S = 0.0f;
    float T = 0.0f;

    // 두 선분 모두 길이가 거의 0이면 점과 점 사이의 거리 문제
    if (A <= EPSILON_1E_8 && E <= EPSILON_1E_8)
    {
        return R.SizeSquared() <= RadiusSum * RadiusSum;
    }
    else if (A <= EPSILON_1E_8) 
    {
        T = FMath::Clamp(FVector::Dot(CapsuleBStart - CapsuleAStart, D2) / E, 0.0f, 1.0f);
    }
    else 
    {
        const float C = FVector::Dot(D1, R);

        if (E <= EPSILON_1E_8)
        {
            S = FMath::Clamp(-C / A, 0.0f, 1.0f);
        }
        else
        {
            const float B = FVector::Dot(D1, D2);
            const float F = FVector::Dot(D2, R);
            const float Denominator = A * E - B * B;

            if (Denominator > EPSILON_1E_8)
            {
                S = FMath::Clamp((B * F - C * E) / Denominator, 0.0f, 1.0f);
            }

            T = (B * S + F) / E;

            if (T < 0.0f)
            {
                T = 0.0f;
                S = FMath::Clamp(-C / A, 0.0f, 1.0f);
            }
            else if (T > 1.0f)
            {
                T = 1.0f;
                S = FMath::Clamp((B - C) / A, 0.0f, 1.0f);
            }
        }
    }

    const FVector ClosestA = CapsuleAStart + D1 * S;
    const FVector ClosestB = CapsuleBStart + D2 * T;

    return (ClosestA - ClosestB).SizeSquared() <= RadiusSum * RadiusSum;
}

bool FCollisionGeometry::IntersectsCapsuleBox(const FVector& CapsuleStart, const FVector& CapsuleEnd, float CapsuleRadius, const FVector& BoxCenter, const FRotator& BoxRotation, const FVector& BoxExtent)
{
    const FQuat InverseRotation = BoxRotation.Quaternion().UnitInverse();
    const FVector Start = InverseRotation.RotateVector(CapsuleStart - BoxCenter);
    const FVector End = InverseRotation.RotateVector(CapsuleEnd - BoxCenter);
    const FVector Segment = Start - End;

    // 선분 위의 점이 박스의 면을 통과하는 t를 구간 경계로 추가
    float Breakpoints[8] = {0.0f, 1.0f};
    int32 Count = 2;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::IsNearlyZero(Segment[Axis]))
        {
            continue;
        }

        const float TMin = (-BoxExtent[Axis] - Start[Axis]) / Segment[Axis];
        const float TMax = (BoxExtent[Axis] - Start[Axis]) / Segment[Axis];

        if (TMin > 0.0f && TMin < 1.0f)
        {
            Breakpoints[Count++] = TMin;
        }

        if (TMax > 0.0f && TMax < 1.0f)
        {
            Breakpoints[Count++] = TMax;
        }
    }

    std::sort(Breakpoints, Breakpoints + Count);

    for (int32 i = 0; i < Count - 1; ++i)
    {
        const float T0 = Breakpoints[i];
        const float T1 = Breakpoints[i + 1];
        const FVector MidPoint = Start + Segment * ((T0 + T1) * 0.5f);
        float Numerator = 0.0f;
        float Denominator = 0.0f;

        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            float Offset;
            if (MidPoint[Axis] < -BoxExtent[Axis])
            {
                Offset = Start[Axis] + BoxExtent[Axis];
            }
            else if (MidPoint[Axis] > BoxExtent[Axis])
            {
                Offset = Start[Axis] - BoxExtent[Axis];
            }
            else
            {
                continue;
            }

            Numerator += Segment[Axis] * Offset;
            Denominator += Segment[Axis] * Segment[Axis];
        }

        float T = T0;

        if (Denominator > EPSILON_1E_8)
        {
            T = FMath::Clamp(-Numerator / Denominator, T0, T1);
        }

        const FVector Point = Start + Segment * T;
        FVector ClosestPoint;

        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            ClosestPoint[Axis] = FMath::Clamp(Point[Axis], -BoxExtent[Axis], BoxExtent[Axis]);
        }
        
        if ((Point - ClosestPoint).SizeSquared() <= CapsuleRadius * CapsuleRadius)
        {
            return true;
        }
    }
    return false;
}

bool FCollisionGeometry::IntersectsBoxBox(const FVector& CenterA, const FRotator& RotationA, const FVector& ExtentA, const FVector& CenterB, const FRotator& RotationB, const FVector& ExtentB)
{
    const FVector LocalAxes[3] = { FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f) };

    const FQuat QuatA = RotationA.Quaternion();
    const FQuat QuatB = RotationB.Quaternion();

    FVector AxesA[3];
    FVector AxesB[3];

    for (int32 i = 0; i < 3; ++i)
    {
        AxesA[i] = QuatA.RotateVector(LocalAxes[i]);
        AxesB[i] = QuatB.RotateVector(LocalAxes[i]);
    }

    FVector TestAxes[15];
    for (int32 i = 0; i < 3; ++i)
    {
        TestAxes[i] = AxesA[i];
        TestAxes[i + 3] = AxesB[i];
    }
    int32 Index = 6;

    for (int32 i = 0; i < 3; ++i)
    {
        for (int32 j = 0; j < 3; ++j)
        {
            TestAxes[Index++] = FVector::Cross(AxesA[i], AxesB[j]);
        }
    }
    const FVector CenterDelta = CenterB - CenterA;

    for (int32 i = 0; i < 15; ++i)
    {
        const FVector& Axis = TestAxes[i];

        if (Axis.SizeSquared() <= EPSILON_1E_8)
        {
            continue;
        }
        float RadiusA = 0.0f;
        float RadiusB = 0.0f;

        for (int32 j = 0; j < 3; ++j)
        {
            RadiusA += ExtentA[j] * std::abs(FVector::Dot(AxesA[j], Axis));
            RadiusB += ExtentB[j] * std::abs(FVector::Dot(AxesB[j], Axis));
        }
        const float Distance = std::abs(FVector::Dot(CenterDelta, Axis));
        if (Distance > RadiusA + RadiusB)
        {
            return false;
        }
    }
    return true;
}
