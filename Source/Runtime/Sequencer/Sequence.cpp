#include "EnginePCH.h"
#include "Sequence.h"

#include <algorithm>
#include <cmath>

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
    const double Alpha = (double(Time) - Left->Time) / (double(Right->Time) - Left->Time);
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
