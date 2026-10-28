#include "EnginePCH.h"
#include "CameraViewInfo.h"

#include <cmath>
bool FCameraPOV::IsValid() const
{
    if (!std::isfinite(Location.X) || !std::isfinite(Location.Y) || !std::isfinite(Location.Z) || !std::isfinite(Rotation.Pitch) || !std::isfinite(Rotation.Yaw) || !std::isfinite(Rotation.Roll))
    {
        return false;
    }

    if (!std::isfinite(FieldOfView) || FieldOfView <= 0.0f || FieldOfView >= 180.0f)
    {
        return false;
    }

    if (!std::isfinite(AspectRatio) || AspectRatio <= 0.0f)
    {
        return false;
    }

    if (!std::isfinite(NearClip) || !std::isfinite(FarClip) || NearClip <= 0.0f || FarClip <= NearClip)
    {
        return false;
    }

    if (!std::isfinite(OrthoWidth) || OrthoWidth <= 0.0f)
    {
        return false;
    }

    if (!std::isfinite(PerspectiveBlend) || PerspectiveBlend < 0.0f || PerspectiveBlend > 1.0f)
    {
        return false;
    }

    return true;
}

FMatrix FCameraPOV::GetViewMatrix() const
{
    const FQuat WorldQuat = Rotation.Quaternion();
    const FMatrix RotationMatrix = WorldQuat.ToFMatrix();

    const FMatrix InverseTranslation = FMatrix::MakeTranslation(Location * -1.0f);
    
    const FMatrix InverseRotation = RotationMatrix.GetTransposed();

    return InverseTranslation * InverseRotation;
}

FMatrix FCameraPOV::GetPerspectiveMatrix() const
{
    const float HalfFOV = FMath::DegreesToRadians(FieldOfView) * 0.5f;

    const float YScale = 1.0f / tan(HalfFOV);
    const float XScale = YScale / AspectRatio;
    const float ZScale = FarClip / (FarClip - NearClip);
    const float ZOffset = -NearClip * FarClip / (FarClip - NearClip);

    return FMatrix(
        0.0f,   0.0f,   ZScale, 1.0f,
        XScale, 0.0f,   0.0f,   0.0f,
        0.0f,   YScale, 0.0f,   0.0f,
        0.0f,   0.0f,   ZOffset,0.0f
    );
}

FMatrix FCameraPOV::GetOrthogonalMatrix() const
{
    const float Width = OrthoWidth;
    const float Height = Width / AspectRatio;

    const float XScale = 2.0f / Width;
    const float YScale = 2.0f / Height;

    const float ZScale = 1.0f / (FarClip - NearClip);
    const float ZOffset = -NearClip / (FarClip - NearClip);

    return FMatrix(
        0.0f,   0.0f,   ZScale, 0.0f,
        XScale, 0.0f,   0.0f,   0.0f,
        0.0f,   YScale, 0.0f,   0.0f,
        0.0f,   0.0f,   ZOffset,1.0f
    );
}

FMatrix FCameraPOV::GetProjectionMatrix() const
{
    const FMatrix PerspectiveMatrix = GetPerspectiveMatrix();
    const FMatrix OrthographicMatrix = GetOrthogonalMatrix();
    const float Alpha = std::clamp(PerspectiveBlend, 0.0f, 1.0f);
    FMatrix Result = FMatrix::Identity;

    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Column = 0; Column < 4; ++Column)
        {
            Result.M[Row][Column] = OrthographicMatrix.M[Row][Column] + (PerspectiveMatrix.M[Row][Column] - OrthographicMatrix.M[Row][Column] * Alpha);
        }
    }
    
    return Result;
}

FMatrix FCameraPOV::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}
