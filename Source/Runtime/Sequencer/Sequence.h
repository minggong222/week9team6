#pragma once

#include <cstddef>
#include <vector>

enum class ESequenceTrackType { Fade };

struct FSequenceKeyframe
{
    float Time = 0.0f; // Absolute sequence time in seconds.
    float Value = 0.0f;
};

struct FSequenceSection
{
    float StartTime = 0.0f;
    float EndTime = 5.0f;

    // Sorted insertion. A key at the exact same time replaces the existing value.
    bool AddKey(float Time, float Value);
    bool Contains(float Time) const;
    bool Evaluate(float Time, float& OutValue) const;
    const std::vector<FSequenceKeyframe>& GetKeys() const { return Keys; }

private:
    std::vector<FSequenceKeyframe> Keys;
};

struct FSequenceTrack
{
    ESequenceTrackType Type = ESequenceTrackType::Fade;
    std::vector<FSequenceSection> Sections;

    // If active sections overlap, the last non-empty section in the list wins.
    bool Evaluate(float Time, float& OutValue, std::size_t& OutSectionIndex) const;
};

struct FSequence
{
    float Duration = 5.0f;
    std::vector<FSequenceTrack> Tracks;
};

struct FSequenceEvaluatedValue
{
    std::size_t TrackIndex = 0;
    std::size_t SectionIndex = 0;
    ESequenceTrackType Type = ESequenceTrackType::Fade;
    float Value = 0.0f;
};
