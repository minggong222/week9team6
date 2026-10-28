#pragma once

#include <cstddef>
#include <vector>
#include <array>
#include <string>

enum class ESequenceTrackType { Fade, Camera };
enum class ESequenceInterpolation { Linear, Smooth, Constant };

struct FSequenceCameraValue
{
    std::array<float, 3> Location{};
    std::array<float, 3> Rotation{}; // Pitch, Yaw, Roll in degrees.
    float FieldOfView = 60.0f;
    bool IsValid() const;
};

struct FSequenceKeyframe
{
    float Time = 0.0f; // Absolute sequence time in seconds.
    float Value = 0.0f;
    FSequenceCameraValue Camera;
};

struct FSequenceFloatChannel
{
    ESequenceInterpolation Interpolation = ESequenceInterpolation::Linear;
    bool AddKey(float Time, float Value);
    bool RemoveKey(float Time);
    bool EditKey(float OldTime, float Time, float Value);
    bool Evaluate(float Time, float& OutValue, bool Angle = false) const;
    const std::vector<FSequenceKeyframe>& GetKeys() const { return Keys; }
private:
    std::vector<FSequenceKeyframe> Keys;
};

struct FSequenceSection
{
    float StartTime = 0.0f;
    float EndTime = 5.0f;
    ESequenceInterpolation Interpolation = ESequenceInterpolation::Linear;
    std::array<FSequenceFloatChannel, 7> CameraChannels; // XYZ, Pitch/Yaw/Roll, FOV.
    FSequenceCameraValue CameraDefaults;
    bool HasCameraKeys() const;

    // Sorted insertion. A key at the exact same time replaces the existing value.
    bool AddKey(float Time, float Value);
    bool AddCameraKey(float Time, const FSequenceCameraValue& Value);
    bool Contains(float Time) const;
    bool Evaluate(float Time, float& OutValue) const;
    bool EvaluateCamera(float Time, FSequenceCameraValue& OutValue) const;
    const std::vector<FSequenceKeyframe>& GetKeys() const { return Keys; }

private:
    std::vector<FSequenceKeyframe> Keys;
};

struct FSequenceTrack
{
    ESequenceTrackType Type = ESequenceTrackType::Fade;
    std::vector<FSequenceSection> Sections;
    std::string CameraActor;
    std::string CameraComponent;

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
    FSequenceCameraValue Camera;
    std::string CameraActor;
    std::string CameraComponent;
};
