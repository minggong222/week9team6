#include "EnginePCH.h"
#include "SequencePlayer.h"

#include <algorithm>
#include <cmath>

bool FSequencePlayer::SetSequence(std::shared_ptr<const FSequence> InSequence)
{
    if (InSequence && (!std::isfinite(InSequence->Duration) || InSequence->Duration <= 0.0f)) return false;
    Sequence = std::move(InSequence);
    if (Sequence)
    {
        PlaybackStart = 0.0f;
        PlaybackEnd = Sequence->Duration;
    }
    Stop();
    return true;
}

void FSequencePlayer::Evaluate()
{
    EvaluatedValues.clear();
    if (!Sequence)
    {
        Apply();
        return;
    }
    for (std::size_t Index = 0; Index < Sequence->Tracks.size(); ++Index)
    {
        const auto& Track = Sequence->Tracks[Index];
        if (Track.Type == ESequenceTrackType::Camera)
        {
            for (std::size_t S = Track.Sections.size(); S > 0; --S)
            {
                FSequenceCameraValue Camera;
                if (!Track.Sections[S - 1].EvaluateCamera(CurrentTime, Camera)) continue;
                FSequenceEvaluatedValue Result;
                Result.TrackIndex = Index; Result.SectionIndex = S - 1; Result.Type = Track.Type;
                Result.Camera = Camera; Result.CameraActor = Track.CameraActor; Result.CameraComponent = Track.CameraComponent;
                EvaluatedValues.push_back(std::move(Result));
                break;
            }
            continue;
        }
        float Value;
        std::size_t SectionIndex;
        if (Track.Evaluate(CurrentTime, Value, SectionIndex))
            EvaluatedValues.push_back({ Index, SectionIndex, Track.Type, Value });
    }
    Apply();
}

void FSequencePlayer::Apply()
{
    std::optional<float> Fade;
    std::optional<FSequenceEvaluatedValue> Camera;
    // Multiple Fade tracks use the last active track's value.
    for (const auto& Result : EvaluatedValues)
    {
        if (Result.Type == ESequenceTrackType::Fade) Fade = std::clamp(Result.Value, 0.0f, 1.0f);
        if (Result.Type == ESequenceTrackType::Camera) Camera = Result;
    }
    if (FadeApplyCallback) FadeApplyCallback(Fade);
    if (CameraApplyCallback) CameraApplyCallback(Camera);
}

void FSequencePlayer::SetCameraApplyCallback(std::function<void(std::optional<FSequenceEvaluatedValue>)> Callback)
{
    if (CameraApplyCallback) CameraApplyCallback(std::nullopt);
    CameraApplyCallback = std::move(Callback);
}

void FSequencePlayer::SetFadeApplyCallback(std::function<void(std::optional<float>)> Callback)
{
    ReleaseAppliedValues();
    FadeApplyCallback = std::move(Callback);
}

void FSequencePlayer::ReleaseAppliedValues()
{
    if (FadeApplyCallback) FadeApplyCallback(std::nullopt);
    if (CameraApplyCallback) CameraApplyCallback(std::nullopt);
}

void FSequencePlayer::Tick(float DeltaTime)
{
    if (!IsPlaying() || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f) return;

    const double DeltaT = double(DeltaTime) * PlaybackSpeed * PlaybackDirection;
    const double Time = double(CurrentTime) + DeltaT;

    const bool bReachedBoundary = PlaybackDirection > 0 ? Time >= PlaybackEnd : Time <= PlaybackStart;
    if (!bReachedBoundary)
    {
        CurrentTime = static_cast<float>(Time);
        Evaluate();
        return;
    }
    if (bLoop)
    {
        const double Span = double(PlaybackEnd) - PlaybackStart;
        double Offset = std::fmod(Time - PlaybackStart, Span);
        if (Offset < 0.0) Offset += Span;
        // At an exact reverse boundary, continue from the opposite (end) boundary.

        CurrentTime = PlaybackDirection < 0 && Offset == 0.0
            ? PlaybackEnd : static_cast<float>(PlaybackStart + Offset);
    }
    else
    {
        CurrentTime = PlaybackDirection > 0 ? PlaybackEnd : PlaybackStart;
        PlaybackState = EPlaybackState::Stopped;
    }
    Evaluate();
}

void FSequencePlayer::Play()
{
    PlaybackDirection = 1;
    PlaybackState = EPlaybackState::Playing;
}

void FSequencePlayer::PlayReverse()
{
    PlaybackDirection = -1;
    PlaybackState = EPlaybackState::Playing;
}

void FSequencePlayer::Pause()
{
    PlaybackState = EPlaybackState::Paused;
}

void FSequencePlayer::Stop()
{
    PlaybackState = EPlaybackState::Stopped;
    CurrentTime = PlaybackStart;
    Evaluate();
}

void FSequencePlayer::Seek(float Time)
{
    if (!std::isfinite(Time)) return;
    CurrentTime = std::clamp(Time, PlaybackStart, PlaybackEnd);
    Evaluate();
}

void FSequencePlayer::StepForward()
{
    Seek(CurrentTime + 1.0f / FramesPerSecond);
    PlaybackState = EPlaybackState::Paused;
}

void FSequencePlayer::StepBackward()
{
    Seek(CurrentTime - 1.0f / FramesPerSecond);
    PlaybackState = EPlaybackState::Paused;
}

void FSequencePlayer::SetPlaybackRange(float Start, float End)
{
    if (!std::isfinite(Start) || !std::isfinite(End) || Start < 0.0f || End <= Start) return;
    PlaybackStart = Start;
    PlaybackEnd = End;
    CurrentTime = std::clamp(CurrentTime, PlaybackStart, PlaybackEnd);
    Evaluate();
}

void FSequencePlayer::SetPlaybackSpeed(float Speed)
{
    if (std::isfinite(Speed) && Speed > 0.0f) PlaybackSpeed = Speed;
}

void FSequencePlayer::SetFramesPerSecond(float Value)
{
    if (std::isfinite(Value) && Value > 0.0f) FramesPerSecond = Value;
}
