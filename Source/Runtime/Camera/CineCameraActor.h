#pragma once

#include "Camera/CameraActor.h"

class ACineCameraActor : public ACameraActor
{
	DECLARE_CLASS(ACineCameraActor, ACameraActor)
    REFLECT_START(ClassName)
        PROPERTY_FLAGS(SensorWidth, CPF_Edit)
        PROPERTY_FLAGS(FocalLength, CPF_Edit)
    REFLECT_END()
public:
    ACineCameraActor();
    float GetSensorWidth() const { return SensorWidth; }
    void SetSensorWidth(float InSensorWidth);
    float GetFocalLength() const { return FocalLength; }
    void SetFocalLength(float InFocalLength);
    void UpdateCameraFOV();
    void PostEditChangeProperty(const FProperty& Property) override;
private:
    // 센서 너미(mm)
    float SensorWidth = 36.0f;
    // 렌즈 초점 거리(mm)
    float FocalLength = 50.0f;
};