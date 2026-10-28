#include "EnginePCH.h"
#include "Sequence.h"

#include <algorithm>
#include <cmath>

namespace
{
    double CurveAlpha(double Alpha, ESequenceInterpolation Mode)
    {
        if (Mode == ESequenceInterpolation::Constant) return 0.0;
        if (Mode == ESequenceInterpolation::Smooth) return Alpha * Alpha * (3.0 - 2.0 * Alpha);
        return Alpha;
    }
}

bool FSequenceCameraValue::IsValid() const
{
    for (float V : Location) if (!std::isfinite(V)) return false;
    for (float V : Rotation) if (!std::isfinite(V)) return false;
    return std::isfinite(FieldOfView) && FieldOfView > 0 && FieldOfView < 180;
}

bool FSequenceFloatChannel::AddKey(float Time, float Value)
{
    if (!std::isfinite(Time) || !std::isfinite(Value) || Time < 0) return false;
    auto Position = std::lower_bound(Keys.begin(), Keys.end(), Time, [](const auto& Key, float T) { return Key.Time < T; });
    if (Position != Keys.end() && Position->Time == Time) Position->Value = Value;
    else Keys.insert(Position, {Time, Value});
    return true;
}

bool FSequenceFloatChannel::RemoveKey(float Time)
{
    auto Position = std::lower_bound(Keys.begin(), Keys.end(), Time, [](const auto& Key, float T) { return Key.Time < T; });
    if (Position == Keys.end() || Position->Time != Time) return false;
    Keys.erase(Position);
    return true;
}

bool FSequenceFloatChannel::EditKey(float OldTime, float Time, float Value)
{
    if (!std::isfinite(Time) || !std::isfinite(Value) || Time < 0) return false;
    if (!RemoveKey(OldTime)) return false;
    return AddKey(Time, Value);
}

bool FSequenceFloatChannel::Evaluate(float Time, float& OutValue, bool Angle) const
{
    if (!std::isfinite(Time) || Keys.empty()) return false;
    if (Time <= Keys.front().Time) { OutValue = Keys.front().Value; return true; }
    if (Time >= Keys.back().Time) { OutValue = Keys.back().Value; return true; }
    const auto Right = std::upper_bound(Keys.begin(), Keys.end(), Time, [](float T, const auto& Key) { return T < Key.Time; });
    const auto Left = Right - 1;
    const double Alpha = CurveAlpha((double(Time) - Left->Time) / (double(Right->Time) - Left->Time), Interpolation);
    double Delta = double(Right->Value) - Left->Value;
    if (Angle) Delta = std::remainder(Delta, 360.0);
    OutValue = static_cast<float>(Left->Value + Delta * Alpha);
    return true;
}

bool FSequenceSection::AddCameraKey(float Time, const FSequenceCameraValue& Value)
{
    if (!std::isfinite(Time) || Time < 0 || !Value.IsValid()) return false;
    for (int I = 0; I < 3; ++I)
    {
        CameraChannels[I].AddKey(Time, Value.Location[I]);
        CameraChannels[I + 3].AddKey(Time, Value.Rotation[I]);
    }
    CameraChannels[6].AddKey(Time, Value.FieldOfView);
    return true;
}

bool FSequenceSection::HasCameraKeys() const
{
    for (const auto& Channel : CameraChannels) if (!Channel.GetKeys().empty()) return true;
    return false;
}

bool FSequenceSection::EvaluateCamera(float Time, FSequenceCameraValue& OutValue) const
{
    if (!Contains(Time) || !HasCameraKeys()) return false;
    OutValue = CameraDefaults;
    for (int I = 0; I < 3; ++I)
    {
        CameraChannels[I].Evaluate(Time, OutValue.Location[I]);
        CameraChannels[I + 3].Evaluate(Time, OutValue.Rotation[I], true);
    }
    CameraChannels[6].Evaluate(Time, OutValue.FieldOfView);
    return true;
}
bool FSequenceSection::AddKey(float Time, float Value)
{
    if (!std::isfinite(Time) || !std::isfinite(Value) || Time < 0.0f) return false;
    const auto Position = std::lower_bound(Keys.begin(), Keys.end(), Time,
        [](const FSequenceKeyframe& Key, float T) { return Key.Time < T; });
    if (Position != Keys.end() && Position->Time == Time) Position->Value = Value;
    else Keys.insert(Position, { Time, Value });
    return true;
}

bool FSequenceSection::Contains(float Time) const
{
    return std::isfinite(Time) && std::isfinite(StartTime) && std::isfinite(EndTime)
        && StartTime >= 0.0f && EndTime >= StartTime && Time >= StartTime && Time <= EndTime;
}

bool FSequenceSection::Evaluate(float Time, float& OutValue) const
{
    if (!Contains(Time) || Keys.empty()) return false;
    if (Time <= Keys.front().Time)
    {
        OutValue = Keys.front().Value;
        return true;
    }
    if (Time >= Keys.back().Time)
    {
        OutValue = Keys.back().Value;
        return true;
    }

    const auto Right = std::upper_bound(Keys.begin(), Keys.end(), Time,
        [](float T, const FSequenceKeyframe& Key) { return T < Key.Time; });
    const auto Left = Right - 1;
    const double Alpha = CurveAlpha((double(Time) - Left->Time) / (double(Right->Time) - Left->Time), Interpolation);
    OutValue = static_cast<float>(Left->Value + (double(Right->Value) - Left->Value) * Alpha);
    return true;
}

bool FSequenceTrack::Evaluate(float Time, float& OutValue, std::size_t& OutSectionIndex) const
{
    for (std::size_t Index = Sections.size(); Index > 0; --Index)
    {
        if (Sections[Index - 1].Evaluate(Time, OutValue))
        {
            OutSectionIndex = Index - 1;
            return true;
        }
    }
    return false;
}
