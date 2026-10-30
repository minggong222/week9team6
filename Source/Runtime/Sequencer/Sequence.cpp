#include "EnginePCH.h"
#include "Sequence.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace
{
    ESequenceInterpolation ReadInterpolation(const std::string& Mode)
    {
        if (Mode == "Linear") return ESequenceInterpolation::Linear;
        if (Mode == "EaseIn") return ESequenceInterpolation::EaseIn;
        if (Mode == "EaseOut") return ESequenceInterpolation::EaseOut;
        if (Mode == "EaseInOut" || Mode == "Smooth") return ESequenceInterpolation::EaseInOut;
        if (Mode == "Constant") return ESequenceInterpolation::Constant;
        throw std::runtime_error("Invalid interpolation mode.");
    }

    std::optional<ESequenceInterpolation> ReadKeyInterpolation(const json& Key)
    {
        if (!Key.contains("interpolation")) return std::nullopt;
        return ReadInterpolation(Key.at("interpolation").get<std::string>());
    }

    bool SetInterpolation(std::vector<FSequenceKeyframe>& Keys, float Time,
        std::optional<ESequenceInterpolation> Mode)
    {
        auto Found = std::lower_bound(Keys.begin(), Keys.end(), Time,
            [](const auto& Key, float T) { return Key.Time < T; });
        if (Found == Keys.end() || Found->Time != Time) return false;
        Found->Interpolation = Mode;
        return true;
    }

    double CurveAlpha(double Alpha, ESequenceInterpolation Mode)
    {
        if (Mode == ESequenceInterpolation::Constant) return 0.0;
        if (Mode == ESequenceInterpolation::EaseIn) return Alpha * Alpha;
        if (Mode == ESequenceInterpolation::EaseOut) return Alpha * (2.0 - Alpha);
        if (Mode == ESequenceInterpolation::EaseInOut) return Alpha * Alpha * (3.0 - 2.0 * Alpha);
        return Alpha;
    }
}

bool FSequenceCameraValue::IsValid() const
{
    for (float V : Location) if (!std::isfinite(V)) return false;
    for (float V : Rotation) if (!std::isfinite(V)) return false;
    return std::isfinite(FieldOfView) && FieldOfView > 0 && FieldOfView < 180 &&
        std::isfinite(Projection) && Projection >= 0.0f && Projection <= 1.0f &&
        std::isfinite(OrthoWidth) && OrthoWidth > 0;
}

float FSequenceCameraValue::GetChannelValue(int Channel) const
{
    if (Channel < 3) return Location[Channel];
    if (Channel < 6) return Rotation[Channel - 3];
    if (Channel == 6) return FieldOfView;
    return Channel == 7 ? Projection : OrthoWidth;
}

float FSequenceCameraValue::ClampChannelValue(int Channel, float Value)
{
    if (Channel == 6) return std::clamp(Value, 1.0f, 179.0f);
    if (Channel == 7) return std::clamp(Value, 0.0f, 1.0f);
    if (Channel == 8) return std::clamp(Value, 0.1f, 100000.0f);
    return Value;
}

bool FSequenceFloatChannel::SetKeyInterpolation(float Time, std::optional<ESequenceInterpolation> Mode)
{
    return SetInterpolation(Keys, Time, Mode);
}

bool FSequenceFloatChannel::AddKey(float Time, float Value, std::optional<ESequenceInterpolation> Mode)
{
    if (!std::isfinite(Time) || !std::isfinite(Value) || Time < 0) return false;
    auto Position = std::lower_bound(Keys.begin(), Keys.end(), Time, [](const auto& Key, float T) { return Key.Time < T; });
    if (Position != Keys.end() && Position->Time == Time)
    {
        Position->Value = Value;
        if (Mode) Position->Interpolation = Mode;
    }
    else Keys.insert(Position, {Time, Value, {}, Mode});
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
    const auto Found = std::find_if(Keys.begin(), Keys.end(),
        [OldTime](const auto& Key) { return Key.Time == OldTime; });
    if (Found == Keys.end()) return false;
    const auto Mode = Found->Interpolation;
    RemoveKey(OldTime);
    AddKey(Time, Value);
    return SetKeyInterpolation(Time, Mode);
}

bool FSequenceFloatChannel::Evaluate(float Time, float& OutValue, bool Angle) const
{
    if (!std::isfinite(Time) || Keys.empty()) return false;
    if (Time <= Keys.front().Time) { OutValue = Keys.front().Value; return true; }
    if (Time >= Keys.back().Time) { OutValue = Keys.back().Value; return true; }
    const auto Right = std::upper_bound(Keys.begin(), Keys.end(), Time, [](float T, const auto& Key) { return T < Key.Time; });
    const auto Left = Right - 1;
    const double Alpha = CurveAlpha((double(Time) - Left->Time) / (double(Right->Time) - Left->Time),
        Left->Interpolation.value_or(Interpolation));
    double Delta = double(Right->Value) - Left->Value;
    if (Angle) Delta = std::remainder(Delta, 360.0);
    OutValue = static_cast<float>(Left->Value + Delta * Alpha);
    return true;
}

FSequenceSection::FSequenceSection()
{
    // 기존 시퀀스와 새 트랙은 기본적으로 즉시 전환합니다.
    CameraChannels[7].Interpolation = ESequenceInterpolation::Constant;
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
    CameraChannels[7].AddKey(Time, Value.Projection);
    CameraChannels[8].AddKey(Time, Value.OrthoWidth);
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
    CameraChannels[7].Evaluate(Time, OutValue.Projection);
    OutValue.Projection = FSequenceCameraValue::ClampChannelValue(7, OutValue.Projection);
    CameraChannels[8].Evaluate(Time, OutValue.OrthoWidth);
    return true;
}
bool FSequenceSection::SetKeyInterpolation(float Time, std::optional<ESequenceInterpolation> Mode)
{
    return SetInterpolation(Keys, Time, Mode);
}

bool FSequenceSection::AddKey(float Time, float Value, std::optional<ESequenceInterpolation> Mode)
{
    if (!std::isfinite(Time) || !std::isfinite(Value) || Time < 0.0f) return false;
    const auto Position = std::lower_bound(Keys.begin(), Keys.end(), Time,
        [](const FSequenceKeyframe& Key, float T) { return Key.Time < T; });
    if (Position != Keys.end() && Position->Time == Time)
    {
        Position->Value = Value;
        if (Mode) Position->Interpolation = Mode;
    }
    else Keys.insert(Position, { Time, Value, {}, Mode });
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
    const double Alpha = CurveAlpha((double(Time) - Left->Time) / (double(Right->Time) - Left->Time),
        Left->Interpolation.value_or(Interpolation));
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

bool LoadSequenceAsset(const std::filesystem::path& Path, FSequenceAsset& OutAsset, std::string& Error)
{
    try
    {
        std::ifstream File(Path, std::ios::binary);
        if (!File) throw std::runtime_error("Could not open file.");
        const json Document = json::parse(File);
        if (!Document.at("version").is_number_integer() ||
            Document.at("version").get<int>() < 1 || Document.at("version").get<int>() > 5)
            throw std::runtime_error("Unsupported Sequence version.");
        const auto Number = [](const json& Value) {
            if (!Value.is_number()) throw std::runtime_error("Expected a number.");
            const float Result = Value.get<float>();
            if (!std::isfinite(Result)) throw std::runtime_error("Invalid number.");
            return Result;
        };
        auto Loaded = std::make_shared<FSequence>();
        Loaded->Duration = Number(Document.at("duration"));
        const float FPS = Number(Document.at("frames_per_second"));
        if (Loaded->Duration < 0.1f || Loaded->Duration > 3600 || FPS < 1 || FPS > 1000)
            throw std::runtime_error("Invalid duration or frame rate.");
        float PlaybackStart = 0.0f;
        float PlaybackEnd = Loaded->Duration;
        if (Document.contains("playback_start") || Document.contains("playback_end"))
        {
            PlaybackStart = Number(Document.at("playback_start"));
            PlaybackEnd = Number(Document.at("playback_end"));
            if (PlaybackStart < 0 || PlaybackEnd <= PlaybackStart || PlaybackEnd > Loaded->Duration)
                throw std::runtime_error("Invalid playback range.");
        }
        const auto& Tracks = Document.at("tracks");
        if (!Tracks.is_array() || Tracks.size() > 2) throw std::runtime_error("At most one Fade and one Camera track are supported.");
        std::vector<std::string> Names;
        bool Seen[2] = {};
        for (const auto& Data : Tracks)
        {
            const int Type = Data.at("type") == "Fade" ? 0 : Data.at("type") == "Camera" ? 1 : -1;
            if (Type < 0 || Seen[Type]) throw std::runtime_error("Unsupported or duplicate track type.");
            Seen[Type] = true;
            FSequenceSection Section;
            Section.StartTime = 0;
            Section.EndTime = Loaded->Duration;
            const std::string Mode = Data.value("interpolation", std::string("Linear"));
            Section.Interpolation = ReadInterpolation(Mode);
            for (auto& Channel : Section.CameraChannels) Channel.Interpolation = Section.Interpolation;
            Section.CameraChannels[7].Interpolation = ESequenceInterpolation::Constant;
            const bool Independent = Type == 1 && Data.contains("channels");
            if (Independent)
            {
                const auto& Defaults = Data.at("defaults");
                const auto& Location = Defaults.at("location"); const auto& Rotation = Defaults.at("rotation");
                if (!Location.is_array() || Location.size() != 3 || !Rotation.is_array() || Rotation.size() != 3)
                    throw std::runtime_error("Invalid camera defaults.");
                for (int I = 0; I < 3; ++I) { Section.CameraDefaults.Location[I] = Number(Location[I]); Section.CameraDefaults.Rotation[I] = Number(Rotation[I]); }
                Section.CameraDefaults.FieldOfView = Number(Defaults.at("fov"));
                if (Defaults.contains("projection")) Section.CameraDefaults.Projection = Number(Defaults.at("projection"));
                if (Defaults.contains("ortho_width")) Section.CameraDefaults.OrthoWidth = Number(Defaults.at("ortho_width"));
                if (!Section.CameraDefaults.IsValid()) throw std::runtime_error("Invalid camera defaults.");
                const auto& Channels = Data.at("channels");
                if (!Channels.is_array() || (Channels.size() != 7 && Channels.size() != 9)) throw std::runtime_error("Invalid camera channels.");
                const char* Names[] = {"Location.X", "Location.Y", "Location.Z", "Pitch", "Yaw", "Roll", "FOV", "Projection", "OrthoWidth"};
                for (int I = 0; I < static_cast<int>(Channels.size()); ++I)
                {
                    const auto& Channel = Channels[I];
                    if (Channel.at("name") != Names[I]) throw std::runtime_error("Invalid camera channel name.");
                    const std::string Interp = Channel.value("interpolation", std::string("Linear"));
                    auto& Curve = Section.CameraChannels[I];
                    Curve.Interpolation = ReadInterpolation(Interp);
                    if (!Channel.at("keys").is_array()) throw std::runtime_error("Invalid channel keys.");
                    for (const auto& Key : Channel.at("keys"))
                    {
                        const float Time = Number(Key.at("time")), Value = Number(Key.at("value"));
                        if ((I == 6 && (Value <= 0 || Value >= 180)) ||
                            (I == 7 && (Value < 0 || Value > 1)) || (I == 8 && Value <= 0) ||
                            !Curve.AddKey(Time, Value, ReadKeyInterpolation(Key))) throw std::runtime_error("Invalid camera channel key.");
                    }
                }
            }
            const auto& Keys = Data.at("keys");
            if (!Keys.is_array()) throw std::runtime_error("Invalid keys.");
            for (const auto& Key : Keys)
            {
                if (Independent) break;
                const float Time = Number(Key.at("time"));
                if (Type == 0)
                {
                    const float Value = Number(Key.at("value"));
                    if (Value < 0 || Value > 1 || !Section.AddKey(Time, Value, ReadKeyInterpolation(Key))) throw std::runtime_error("Invalid Fade key.");
                }
                else
                {
                    FSequenceCameraValue Value;
                    const auto& Location = Key.at("location"); const auto& Rotation = Key.at("rotation");
                    if (!Location.is_array() || Location.size() != 3 || !Rotation.is_array() || Rotation.size() != 3)
                        throw std::runtime_error("Invalid camera transform.");
                    for (int I = 0; I < 3; ++I) { Value.Location[I] = Number(Location[I]); Value.Rotation[I] = Number(Rotation[I]); }
                    Value.FieldOfView = Number(Key.at("fov"));
                    if (!Section.AddCameraKey(Time, Value)) throw std::runtime_error("Invalid camera key.");
                }
            }
            FSequenceTrack Track;
            Track.Type = static_cast<ESequenceTrackType>(Type);
            if (Type == 1)
            {
                Track.CameraActor = Data.at("camera_actor").get<std::string>();
                Track.CameraComponent = Data.at("camera_component").get<std::string>();
            }
            Track.Sections.push_back(std::move(Section));
            Loaded->Tracks.push_back(std::move(Track));
            Names.push_back(Data.at("name").get<std::string>());
        }
        OutAsset = {std::move(Loaded), FPS, PlaybackStart, PlaybackEnd, std::move(Names)};
        Error.clear();
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}
