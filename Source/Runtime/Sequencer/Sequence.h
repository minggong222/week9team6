#pragma once

#include <cstddef>
#include <vector>
#include <array>
#include <string>
#include <optional>
#include <memory>
#include <filesystem>

enum class ESequenceTrackType { Fade, Camera };
enum class ESequenceInterpolation
{
    Linear, EaseInOut, Constant, EaseIn, EaseOut,
    Smooth = EaseInOut // 기존 Smooth 설정 및 호출과 호환됩니다.
};

struct FSequenceCameraValue
{
    std::array<float, 3> Location{};
    std::array<float, 3> Rotation{}; // Pitch, Yaw, Roll in degrees.
    float FieldOfView = 60.0f;
    float Projection = 0.0f; // 0: 원근, 1: 직교. 중간값은 두 투영의 혼합 비율입니다.
    float OrthoWidth = 16.0f;
    float GetChannelValue(int Channel) const;
    static float ClampChannelValue(int Channel, float Value);
    bool IsValid() const;
};

struct FSequenceKeyframe
{
    float Time = 0.0f; // Absolute sequence time in seconds.
    float Value = 0.0f;
    FSequenceCameraValue Camera;
    // 이 키에서 다음 키까지 적용합니다. 미지정이면 채널 기본값을 따릅니다.
    std::optional<ESequenceInterpolation> Interpolation;
};

struct FSequenceFloatChannel
{
    ESequenceInterpolation Interpolation = ESequenceInterpolation::Linear;
    bool AddKey(float Time, float Value, std::optional<ESequenceInterpolation> Mode = std::nullopt);
    bool SetKeyInterpolation(float Time, std::optional<ESequenceInterpolation> Mode);
    bool RemoveKey(float Time);
    bool EditKey(float OldTime, float Time, float Value);
    bool Evaluate(float Time, float& OutValue, bool Angle = false) const;
    const std::vector<FSequenceKeyframe>& GetKeys() const { return Keys; }
private:
    std::vector<FSequenceKeyframe> Keys;
};

struct FSequenceSection
{
    FSequenceSection();
    float StartTime = 0.0f;
    float EndTime = 5.0f;
    ESequenceInterpolation Interpolation = ESequenceInterpolation::Linear;
    std::array<FSequenceFloatChannel, 9> CameraChannels; // XYZ, Pitch/Yaw/Roll, FOV, Projection, OrthoWidth.
    FSequenceCameraValue CameraDefaults;
    bool HasCameraKeys() const;

    // Sorted insertion. A key at the exact same time replaces the existing value.
    bool AddKey(float Time, float Value, std::optional<ESequenceInterpolation> Mode = std::nullopt);
    bool SetKeyInterpolation(float Time, std::optional<ESequenceInterpolation> Mode);
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

struct FSequenceAsset
{
    std::shared_ptr<FSequence> Sequence;
    float FramesPerSecond = 30.0f;
    float PlaybackStart = 0.0f;
    float PlaybackEnd = 5.0f;
    std::vector<std::string> TrackNames;
};

// 에디터와 게임이 동일한 검증·보간·파일 버전을 사용합니다.
bool LoadSequenceAsset(const std::filesystem::path& Path, FSequenceAsset& OutAsset, std::string& Error);

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
