#pragma once

#include "Sequence.h"
#include <memory>
#include <functional>
#include <optional>

enum class EPlaybackState
{
    Stopped,
    Playing,
    Paused
};

// Owns playback state and time independently of the editor UI.
class FSequencePlayer
{
public:
    bool SetSequence(std::shared_ptr<const FSequence> InSequence);
    const std::shared_ptr<const FSequence>& GetSequence() const { return Sequence; }
    void Evaluate();
    void Apply();
    // An empty value releases Sequence control when no Fade section is active.
    void SetFadeApplyCallback(std::function<void(std::optional<float>)> Callback);
    void SetCameraApplyCallback(std::function<void(std::optional<FSequenceEvaluatedValue>)> Callback);
    void ReleaseAppliedValues();
    const std::vector<FSequenceEvaluatedValue>& GetEvaluatedValues() const { return EvaluatedValues; }
    // The caller chooses real or game-scaled seconds; do not apply TimeScale again here.
    void Tick(float DeltaTime);
    void Play();
    void PlayReverse();
    void Pause();
    void Stop();
    void Seek(float Time);
    void StepForward();
    void StepBackward();
    void SetPlaybackRange(float Start, float End);
    void SetPlaybackSpeed(float Speed);
    void SetFramesPerSecond(float Value);
    void SetLoop(bool bEnabled) { bLoop = bEnabled; }

    EPlaybackState GetPlaybackState() const { return PlaybackState; }
    bool IsPlaying() const { return PlaybackState == EPlaybackState::Playing; }
    float GetCurrentTime() const { return CurrentTime; }
    float GetPlaybackStart() const { return PlaybackStart; }
    float GetPlaybackEnd() const { return PlaybackEnd; }
    int GetPlaybackDirection() const { return PlaybackDirection; }
    float GetPlaybackSpeed() const { return PlaybackSpeed; }
    float GetFramesPerSecond() const { return FramesPerSecond; }
    bool IsLooping() const { return bLoop; }

private:
    std::shared_ptr<const FSequence> Sequence;
    std::vector<FSequenceEvaluatedValue> EvaluatedValues;
    std::function<void(std::optional<float>)> FadeApplyCallback;
    std::function<void(std::optional<FSequenceEvaluatedValue>)> CameraApplyCallback;
    EPlaybackState PlaybackState = EPlaybackState::Stopped;
    float CurrentTime = 0.0f;
    float PlaybackStart = 0.0f;
    float PlaybackEnd = 5.0f;
    int PlaybackDirection = 1;
    float PlaybackSpeed = 1.0f;
    float FramesPerSecond = 30.0f;
    bool bLoop = false;
};
