#include "EnginePCH.h"
#include "SequencerPanel.h"
#include "Core/EngineTimer.h"
#include "Camera/PlayerCameraManager.h"
#include "ThirdParty/ImCurveEdit/ImCurveEdit.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Camera/ViewportCameraMovement.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace
{
    const char* InterpolationName(ESequenceInterpolation Mode)
    {
        switch (Mode)
        {
        case ESequenceInterpolation::EaseIn: return "EaseIn";
        case ESequenceInterpolation::EaseOut: return "EaseOut";
        case ESequenceInterpolation::EaseInOut: return "EaseInOut";
        case ESequenceInterpolation::Constant: return "Constant";
        default: return "Linear";
        }
    }

    constexpr float TimelineMarkerRowHeight = 24.0f;
    constexpr float TimelinePrerollSeconds = 0.5f;

    int GetTimelineFirstFrame(float FPS)
    {
        return -std::max(1, static_cast<int>(std::ceil(FPS * TimelinePrerollSeconds)));
    }

    enum class ETransportIcon
    {
        Record, RangeStart, First, PreviousKey, PreviousFrame, Reverse,
        Play, Pause, NextFrame, NextKey, Last, RangeEnd, Once, Loop
    };

    void NextToolbarControl(float Width)
    {
        const float Right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + Width <= Right)
            ImGui::SameLine();
    }

    bool TransportButton(const char* Id, const char* Tooltip, ETransportIcon Icon, bool Active = false)
    {
        const float Size = ImGui::GetFrameHeight();
        ImGui::PushStyleColor(ImGuiCol_Button, Active ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive) : ImVec4(0, 0, 0, 0));
        const bool Clicked = ImGui::Button(Id, ImVec2(Size, Size));
        ImGui::PopStyleColor();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const float X = Min.x + Size * 0.5f;
        const float Y = Min.y + Size * 0.5f;
        const float R = Size * 0.22f;
        ImDrawList* Draw = ImGui::GetWindowDrawList();
        const ImU32 Color = ImGui::GetColorU32(ImGuiCol_Text);
        if (Icon == ETransportIcon::Pause)
        {
            Draw->AddRectFilled(ImVec2(X - R, Y - R), ImVec2(X - R * 0.3f, Y + R), Color);
            Draw->AddRectFilled(ImVec2(X + R * 0.3f, Y - R), ImVec2(X + R, Y + R), Color);
        }
        else if (Icon == ETransportIcon::Record)
        {
            Draw->AddCircle(ImVec2(X, Y), R * 1.6f, Color, 20, 1.5f);
            Draw->AddCircleFilled(ImVec2(X, Y), R, ImGui::GetColorU32(ImVec4(0.85f, 0.18f, 0.18f, 1)), 20);
        }
        else if (Icon == ETransportIcon::RangeStart || Icon == ETransportIcon::RangeEnd)
        {
            const float D = Icon == ETransportIcon::RangeStart ? 1.0f : -1.0f;
            const ImU32 Marker = ImGui::GetColorU32(D > 0 ? ImVec4(0.25f, 0.8f, 0.35f, 1) : ImVec4(0.9f, 0.2f, 0.2f, 1));
            Draw->AddLine(ImVec2(X, Y - R * 1.4f), ImVec2(X, Y + R * 1.4f), Marker, 2);
            Draw->AddTriangleFilled(ImVec2(X, Y - R * 1.4f), ImVec2(X + D * R, Y - R * 1.4f), ImVec2(X, Y - R * 0.4f), Marker);
            Draw->AddTriangleFilled(ImVec2(X, Y + R * 1.4f), ImVec2(X + D * R, Y + R * 1.4f), ImVec2(X, Y + R * 0.4f), Marker);
        }
        else if (Icon == ETransportIcon::Once)
        {
            Draw->AddRectFilled(ImVec2(X - R * 1.5f, Y - R * 0.4f), ImVec2(X + R * 0.3f, Y + R * 0.4f), Color);
            Draw->AddTriangleFilled(ImVec2(X, Y - R * 1.2f), ImVec2(X, Y + R * 1.2f), ImVec2(X + R * 1.5f, Y), Color);
        }
        else if (Icon == ETransportIcon::Loop)
        {
            Draw->AddRect(ImVec2(X - R * 1.5f, Y - R), ImVec2(X + R * 1.5f, Y + R), Color, R * 0.6f, 2.0f);
            Draw->AddTriangleFilled(ImVec2(X, Y + R * 0.3f), ImVec2(X, Y + R * 1.7f), ImVec2(X + R, Y + R), Color);
        }
        else if (Icon == ETransportIcon::PreviousKey || Icon == ETransportIcon::NextKey)
        {
            const float D = Icon == ETransportIcon::PreviousKey ? -1.0f : 1.0f;
            Draw->AddLine(ImVec2(X - D * R * 0.4f, Y - R), ImVec2(X - D * R * 1.4f, Y), Color, 2);
            Draw->AddLine(ImVec2(X - D * R * 1.4f, Y), ImVec2(X - D * R * 0.4f, Y + R), Color, 2);
            const float KeyX = X + D * R * 0.5f;
            Draw->AddQuadFilled(ImVec2(KeyX, Y - R * 0.6f), ImVec2(KeyX + R * 0.6f, Y),
                ImVec2(KeyX, Y + R * 0.6f), ImVec2(KeyX - R * 0.6f, Y), Color);
        }
        else
        {
            const bool Backward = Icon == ETransportIcon::First || Icon == ETransportIcon::PreviousFrame || Icon == ETransportIcon::Reverse;
            const float D = Backward ? -1.0f : 1.0f;
            Draw->AddTriangleFilled(ImVec2(X - D * R * 0.7f, Y - R),
                ImVec2(X - D * R * 0.7f, Y + R), ImVec2(X + D * R, Y), Color);
            if (Icon != ETransportIcon::Play && Icon != ETransportIcon::Reverse)
            {
                const bool IsFrame = Icon == ETransportIcon::PreviousFrame || Icon == ETransportIcon::NextFrame;
                const float BarX = X + (IsFrame ? -D : D) * R * 1.3f;
                Draw->AddLine(ImVec2(BarX, Y - R), ImVec2(BarX, Y + R), Color, 2);
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tooltip);
        return Clicked;
    }
}

bool FSequencerPanel::Init()
{
    std::error_code FolderError;
    std::filesystem::create_directories("Assets/Sequences", FolderError);
    Player.SetCameraApplyCallback([this](std::optional<FSequenceEvaluatedValue> Result) { ApplyCameraResult(std::move(Result)); });
    NewSequence();
    return true;
}

void FSequencerPanel::NewSequence()
{
    Player.ReleaseAppliedValues();
    PreviewSequence = std::make_shared<FSequence>();
    EditorTracks.clear();
    ResetEditorState(30.0f);
    NextEditorId = 1;
    bExpanded = true;
    SequencePath.clear();
    Player.SetSequence(PreviewSequence);
    AddTrack(0);
}

void FSequencerPanel::ResetEditorState(float FPS)
{
    PreviewData = FSequencerPreviewData{};
    PreviewData.FirstFrame = GetTimelineFirstFrame(FPS);
    TimeTransform = FSequencerTimeTransform{};
    SelectedTrack = -1;
    SelectedKeyTime.reset();
    bDraggingKey = false;
    bDraggingTrackBoundary = false;
    DraggedTimelineHandle = 0;
    ContextTrackId = 0;
    bTimelineContext = false;
    ContextTime = 0;
    bCameraPreviewEnabled = false;
    bCameraPilotEnabled = false;
    bCameraExpanded = bCameraLocationExpanded = bCameraRotationExpanded = true;
    CurveTrackId = -1;
    CurvePointCount = -1;
    ++CurveEditorSerial;
    PilotCameraValue.reset();
    FirstVisibleFrame = PreviewData.GetFrameMin();
    FileError.clear();
    Player.SetFramesPerSecond(FPS);
    Player.SetPlaybackSpeed(1);
    Player.SetLoop(false);
}

void FSequencerPanel::SetCameraManagerProvider(std::function<APlayerCameraManager*()> Provider)
{
    Player.ReleaseAppliedValues();
    CameraManagerProvider = std::move(Provider);
    Player.SetFadeApplyCallback([this](std::optional<float> Value)
    {
        bPreviewRequested = Value.has_value();
        if (FadePreviewCallback) FadePreviewCallback(Value);
        APlayerCameraManager* Manager = CameraManagerProvider ? CameraManagerProvider() : nullptr;
        if (!Manager) return;
        if (Value) Manager->SetSequenceFade(&Player, *Value);
        else Manager->ReleaseSequenceFade(&Player);
    });
}

std::vector<FSequenceSection>& FSequencerPanel::GetSections(int TrackIndex)
{
    auto& Track = EditorTracks[TrackIndex];
    return PreviewSequence->Tracks[Track.RuntimeTrackIndex].Sections;
}

int FSequencerPanel::GetSelectedTrackIndex() const
{
    return SelectedTrack >= 0 && SelectedTrack < PreviewData.GetItemCount()
        ? PreviewData.Items[SelectedTrack].TrackIndex : -1;
}

void FSequencerPanel::NotifySequenceEdited()
{
    PilotCameraValue.reset();
    // Re-evaluate without resetting time, direction, or playback state.
    Player.Evaluate();
}

void FSequencerPanel::RebuildItemsFromSequence(int TrackId, int SectionId)
{
    if (TrackId < 0 && SelectedTrack >= 0 && SelectedTrack < PreviewData.GetItemCount())
    {
        TrackId = PreviewData.Items[SelectedTrack].TrackId;
        SectionId = PreviewData.Items[SelectedTrack].Id;
    }
    PreviewData.Items.clear();
    const float FPS = Player.GetFramesPerSecond();
    for (int TrackIndex = 0; TrackIndex < static_cast<int>(EditorTracks.size()); ++TrackIndex)
    {
        auto& Track = EditorTracks[TrackIndex];
        const auto& Sections = GetSections(TrackIndex);
        Track.SectionIds.resize(Sections.size(), 0);
        for (int& Id : Track.SectionIds) if (Id == 0) Id = NextEditorId++;
        if (Sections.empty())
        {
            FSequencerPreviewData::FItem Row;
            Row.Type = Track.Type; Row.Name = Track.Name; Row.Id = -Track.Id;
            Row.TrackId = Track.Id; Row.TrackIndex = TrackIndex;
            PreviewData.Items.push_back(std::move(Row));

        }
        for (int SectionIndex = 0; SectionIndex < static_cast<int>(Sections.size()); ++SectionIndex)
        {
            const auto& Section = Sections[SectionIndex];
            FSequencerPreviewData::FItem Row;
            Row.Type = Track.Type;
            Row.Name = Track.Name;
            Row.Id = Track.SectionIds[SectionIndex];
            Row.TrackId = Track.Id;
            Row.TrackIndex = TrackIndex;
            Row.SectionIndex = SectionIndex;
            Row.Start = static_cast<int>(std::lround(Section.StartTime * FPS));
            Row.End = static_cast<int>(std::lround(Section.EndTime * FPS));
            if (Track.Type != 1) PreviewData.Items.push_back(Row);
            if (Track.Type == 1)
            {
                Row.Group = 3;
                PreviewData.Items.push_back(Row);
                if (!bCameraExpanded) continue;

                const auto Parent = Row;
                const auto Child = [&](const char* Name, int Channel, int Group, int Offset) {
                    auto Item = Parent;
                    Item.Name = Name; Item.Channel = Channel; Item.Group = Group;
                    Item.Id = -(Parent.Id * 16 + Offset);
                    PreviewData.Items.push_back(std::move(Item));
                };
                Child("Location", -1, 1, 1);
                if (bCameraLocationExpanded)
                {
                    Child("X", 0, 0, 2); Child("Y", 1, 0, 3); Child("Z", 2, 0, 4);
                }
                Child("Rotation", -1, 2, 5);
                if (bCameraRotationExpanded)
                {
                    Child("Roll", 5, 0, 6); Child("Pitch", 3, 0, 7); Child("Yaw", 4, 0, 8);
                }
                Child("FOV", 6, 0, 9);
                Child("Projection", 7, 0, 10);
                Child("Ortho Width", 8, 0, 11);
            }
        }
    }
    SelectedTrack = -1;
    for (int Index = 0; Index < PreviewData.GetItemCount(); ++Index)
    {
        const auto& Row = PreviewData.Items[Index];
        if (Row.TrackId == TrackId && (SelectedTrack < 0 || Row.Id == SectionId)) SelectedTrack = Index;
        if (Row.TrackId == TrackId && Row.Id == SectionId) break;
    }
    // 접힌 하위 행의 선택이 상위 그룹 전체 선택으로 바뀌지 않도록 합니다.
    if (SelectedTrack < 0 || PreviewData.Items[SelectedTrack].Id != SectionId)
    {
        SelectedKeyTime.reset();
        bDraggingKey = false;
    }
    CachedFrameRate = FPS;
    PreviewData.Rows.clear();
    PreviewData.bHasGeometry = false;
}

void FSequencerPanel::AddTrack(int Type)
{
    if (Type < 0 || Type > 1) return;
    for (const auto& Existing : EditorTracks)
    {
        if (Existing.Type == Type)
        {
            RebuildItemsFromSequence(Existing.Id, 0);
            return;
        }
    }
    SelectedKeyTime.reset();
    FEditorTrack Track;
    Track.Id = NextEditorId++;
    Track.Type = Type;
    Track.Name = Type == 0 ? "Fade" : "Camera";
    Track.RuntimeTrackIndex = static_cast<int>(PreviewSequence->Tracks.size());
    PreviewSequence->Tracks.push_back(FSequenceTrack{});
    PreviewSequence->Tracks.back().Type = static_cast<ESequenceTrackType>(Type);
    EditorTracks.push_back(std::move(Track));
    const int Index = static_cast<int>(EditorTracks.size()) - 1;
    FSequenceSection Section;
    Section.StartTime = 0;
    Section.EndTime = PreviewSequence->Duration;
    GetSections(Index).push_back(std::move(Section));
    EditorTracks[Index].SectionIds.push_back(NextEditorId++);
    RebuildItemsFromSequence(EditorTracks[Index].Id, EditorTracks[Index].SectionIds.back());
    NotifySequenceEdited();
}

void FSequencerPanel::DeleteTrack(int TrackIndex)
{
    if (TrackIndex < 0 || TrackIndex >= static_cast<int>(EditorTracks.size())) return;
    SelectedKeyTime.reset();
    bDraggingKey = false;
    const int RuntimeIndex = EditorTracks[TrackIndex].RuntimeTrackIndex;
    if (RuntimeIndex >= 0)
    {
        PreviewSequence->Tracks.erase(PreviewSequence->Tracks.begin() + RuntimeIndex);
        for (auto& Track : EditorTracks) if (Track.RuntimeTrackIndex > RuntimeIndex) --Track.RuntimeTrackIndex;
    }
    EditorTracks.erase(EditorTracks.begin() + TrackIndex);
    const int NextTrack = EditorTracks.empty() ? 0 : EditorTracks[std::min(TrackIndex, static_cast<int>(EditorTracks.size()) - 1)].Id;
    RebuildItemsFromSequence(NextTrack, 0);
    NotifySequenceEdited();
}

FSequenceSection* FSequencerPanel::GetSelectedSection()
{
    const int TrackIndex = GetSelectedTrackIndex();
    if (TrackIndex < 0 || TrackIndex >= static_cast<int>(EditorTracks.size())) return nullptr;
    auto& Sections = GetSections(TrackIndex);
    return Sections.empty() ? nullptr : &Sections.front();
}

std::vector<int> FSequencerPanel::GetRowChannels(int RowIndex) const
{
    if (RowIndex < 0 || RowIndex >= PreviewData.GetItemCount()) return {};
    const auto& Item = PreviewData.Items[RowIndex];
    if (Item.Type != 1) return {};
    if (Item.Channel >= 0) return {Item.Channel};
    if (Item.Group == 1) return {0, 1, 2};
    if (Item.Group == 2) return {3, 4, 5};
    if (Item.Group == 3) return {0, 1, 2, 3, 4, 5, 6, 7, 8};
    return {};
}

std::vector<FSequenceKeyframe> FSequencerPanel::GetRowKeys(int RowIndex) const
{
    if (RowIndex < 0 || RowIndex >= PreviewData.GetItemCount()) return {};
    const auto& Item = PreviewData.Items[RowIndex];
    const auto& Track = EditorTracks[Item.TrackIndex];
    const auto& Sections = PreviewSequence->Tracks[Track.RuntimeTrackIndex].Sections;
    if (Item.SectionIndex < 0 || Item.SectionIndex >= static_cast<int>(Sections.size())) return {};
    const auto& Section = Sections[Item.SectionIndex];
    if (Item.Type == 0) return Section.GetKeys();
    std::vector<FSequenceKeyframe> Keys;
    for (int Channel : GetRowChannels(RowIndex))
        for (const auto& Key : Section.CameraChannels[Channel].GetKeys()) Keys.push_back(Key);
    std::stable_sort(Keys.begin(), Keys.end(), [](const auto& A, const auto& B) { return A.Time < B.Time; });
    Keys.erase(std::unique(Keys.begin(), Keys.end(), [](const auto& A, const auto& B) { return A.Time == B.Time; }), Keys.end());
    return Keys;
}

void FSequencerPanel::SelectCameraChannel(int Channel)
{
    const int TrackIndex = GetSelectedTrackIndex();
    if (TrackIndex < 0 || EditorTracks[TrackIndex].Type != 1 || Channel < 0 || Channel > 8) return;
    const int TrackId = EditorTracks[TrackIndex].Id;
    if (!bCameraExpanded || (Channel < 3 && !bCameraLocationExpanded) ||
        (Channel >= 3 && Channel < 6 && !bCameraRotationExpanded))
    {
        bCameraExpanded = true;
        if (Channel < 3) bCameraLocationExpanded = true;
        if (Channel >= 3 && Channel < 6) bCameraRotationExpanded = true;
        RebuildItemsFromSequence(TrackId, 0);
    }
    for (int I = 0; I < PreviewData.GetItemCount(); ++I)
        if (PreviewData.Items[I].TrackId == TrackId && PreviewData.Items[I].Channel == Channel)
        { if (SelectedTrack != I) SelectedKeyTime.reset(); SelectedTrack = I; break; }
    CurveChannel = Channel;
}
void FSequencerPanel::AddKey(float Time)
{
    auto* Section = GetSelectedSection();
    if (!Section || !std::isfinite(Time)) return;
    Time = std::clamp(Time, 0.0f, PreviewSequence->Duration);
    if (EditorTracks[GetSelectedTrackIndex()].Type == 1)
    {
        const auto Pose = IsCameraPilotEnabled() && PilotCameraValue && Time == PilotCameraTime ? *PilotCameraValue : GetCameraKeyValue(Time);
        if (!Section->HasCameraKeys()) Section->CameraDefaults = GetCameraKeyValue(Time);
        for (int Channel : GetRowChannels(SelectedTrack))
            Section->CameraChannels[Channel].AddKey(Time, Pose.GetChannelValue(Channel));
    }
    else
    {
        float Value = 0.0f;
        Section->Evaluate(Time, Value);
        Section->AddKey(Time, std::clamp(Value, 0.0f, 1.0f));
    }
    SelectedKeyTime = Time;
    NotifySequenceEdited();
}

void FSequencerPanel::SetSelectedKeyInterpolation(std::optional<ESequenceInterpolation> Mode)
{
    auto* Section = GetSelectedSection();
    if (!Section || !SelectedKeyTime) return;
    bool Changed = false;
    if (EditorTracks[GetSelectedTrackIndex()].Type == 1)
    {
        for (int Channel : GetRowChannels(SelectedTrack))
            Changed |= Section->CameraChannels[Channel].SetKeyInterpolation(*SelectedKeyTime, Mode);
    }
    else Changed = Section->SetKeyInterpolation(*SelectedKeyTime, Mode);
    if (Changed) NotifySequenceEdited();
}

void FSequencerPanel::DrawSelectedKeyInterpolation()
{
    auto* Section = GetSelectedSection();
    if (!Section || !SelectedKeyTime) return;
    int Selection = -2;
    const auto Observe = [&](const std::vector<FSequenceKeyframe>& Keys) {
        const auto Found = std::find_if(Keys.begin(), Keys.end(),
            [this](const auto& Key) { return Key.Time == *SelectedKeyTime; });
        if (Found == Keys.end()) return;
        const int Mode = Found->Interpolation ? static_cast<int>(*Found->Interpolation) + 1 : 0;
        Selection = Selection == -2 ? Mode : Selection == Mode ? Selection : -1;
    };
    const bool Camera = EditorTracks[GetSelectedTrackIndex()].Type == 1;
    if (Camera)
    {
        for (int Channel : GetRowChannels(SelectedTrack)) Observe(Section->CameraChannels[Channel].GetKeys());
    }
    else Observe(Section->GetKeys());
    if (Selection == -2) return;

    const bool Children = Camera && PreviewData.Items[SelectedTrack].Group != 0;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(Children ? "Key + children -> next" : "Key -> next");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    const char* Names[] = {"Use default", "Linear", "Ease In Out", "Step", "Ease In", "Ease Out"};
    if (ImGui::BeginCombo("##KeyInterpolation", Selection < 0 ? "Mixed" : Names[Selection]))
    {
        for (int Index = 0; Index < 6; ++Index)
            if (ImGui::Selectable(Names[Index], Selection == Index))
                SetSelectedKeyInterpolation(Index == 0 ? std::nullopt :
                    std::optional<ESequenceInterpolation>(static_cast<ESequenceInterpolation>(Index - 1)));
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Apply from the selected key (%.3fs) to the next key. %s", *SelectedKeyTime,
            Children ? "Includes existing child keys at the same time." : "Only this key is changed.");
}

void FSequencerPanel::DeleteSelectedKey()
{
    auto* Section = GetSelectedSection();
    if (!Section || !SelectedKeyTime) return;
    if (EditorTracks[GetSelectedTrackIndex()].Type == 1)
    {
        for (int Channel : GetRowChannels(SelectedTrack)) Section->CameraChannels[Channel].RemoveKey(*SelectedKeyTime);
        SelectedKeyTime.reset(); bDraggingKey = false;
        NotifySequenceEdited();
        return;
    }
    FSequenceSection Updated;
    Updated.StartTime = Section->StartTime;
    Updated.EndTime = Section->EndTime;
    Updated.Interpolation = Section->Interpolation;
    for (const auto& Key : Section->GetKeys())
        if (Key.Time != *SelectedKeyTime)
        {
            if (EditorTracks[GetSelectedTrackIndex()].Type == 1) Updated.AddCameraKey(Key.Time, Key.Camera);
            else Updated.AddKey(Key.Time, Key.Value, Key.Interpolation);
        }
    *Section = std::move(Updated);
    SelectedKeyTime.reset();
    bDraggingKey = false;
    NotifySequenceEdited();
}

void FSequencerPanel::EditSelectedKey(float Time, float Value)
{
    auto* Section = GetSelectedSection();
    if (!Section || !SelectedKeyTime || !std::isfinite(Time) || !std::isfinite(Value)) return;
    if (EditorTracks[GetSelectedTrackIndex()].Type == 1)
    {
        Time = std::clamp(Time, 0.0f, PreviewSequence->Duration);
        const bool Leaf = PreviewData.Items[SelectedTrack].Channel >= 0;
        for (int Channel : GetRowChannels(SelectedTrack))
        {
            auto& Curve = Section->CameraChannels[Channel];
            const auto Keys = Curve.GetKeys();
            const auto Found = std::find_if(Keys.begin(), Keys.end(), [this](const auto& Key) { return Key.Time == *SelectedKeyTime; });
            if (Found == Keys.end()) continue;
            const float NewValue = Leaf ? FSequenceCameraValue::ClampChannelValue(Channel, Value) : Found->Value;
            Curve.EditKey(*SelectedKeyTime, Time, NewValue);
        }
        SelectedKeyTime = Time;
        NotifySequenceEdited();
        return;
    }
    Time = std::clamp(Time, 0.0f, PreviewSequence->Duration);
    FSequenceSection Updated;
    Updated.StartTime = Section->StartTime;
    Updated.EndTime = Section->EndTime;
    Updated.Interpolation = Section->Interpolation;
    std::optional<ESequenceInterpolation> Mode;
    for (const auto& Key : Section->GetKeys())
        if (Key.Time != *SelectedKeyTime) Updated.AddKey(Key.Time, Key.Value, Key.Interpolation);
        else Mode = Key.Interpolation;
    // Moving onto an existing key replaces its value.
    Updated.AddKey(Time, std::clamp(Value, 0.0f, 1.0f));
    Updated.SetKeyInterpolation(Time, Mode);
    *Section = std::move(Updated);
    SelectedKeyTime = Time;
    NotifySequenceEdited();
}

void FSequencerPanel::SyncSequence()
{
    bool Changed = false;
    for (int Index = 0; Index < static_cast<int>(EditorTracks.size()); ++Index)
    {
        auto& Sections = GetSections(Index);
        if (Sections.empty()) { Sections.emplace_back(); Changed = true; }
        auto& Section = Sections.front();
        if (Section.StartTime != 0 || Section.EndTime != PreviewSequence->Duration)
        {
            Section.StartTime = 0;
            Section.EndTime = PreviewSequence->Duration;
            Changed = true;
        }
    }
    if (SelectedKeyTime && *SelectedKeyTime > PreviewSequence->Duration)
    {
        SelectedKeyTime.reset();
        bDraggingKey = false;
    }
    if (Changed || CachedFrameRate != Player.GetFramesPerSecond()) RebuildItemsFromSequence();
    if (Changed) NotifySequenceEdited();
}

void FSequencerPanel::ReleaseEditorCameraPreview()
{
    bCameraPreviewEnabled = false;
    bCameraPilotEnabled = false;
    PilotCameraValue.reset();
    if (CameraPreviewCallback) CameraPreviewCallback(std::nullopt);
}

void FSequencerPanel::ReleasePreview()
{
    Player.Pause();
    ReleaseEditorCameraPreview();
    Player.ReleaseAppliedValues();
    bPreviewRequested = false;
    bCameraPreviewRequested = false;
}

void FSequencerPanel::Tick(float /*DeltaTime*/)
{
    if (IsOpen())
    {
        SyncSequence();
        const bool bWasPlaying = Player.IsPlaying();
        Player.Tick(EngineTimer::GetRealDeltaTime());
        if ((bPreviewRequested || bCameraPreviewRequested) && !bWasPlaying) Player.Apply();
    }
    else ReleasePreview();
}

void FSequencerPanel::SetInitialDock()
{
    if (!bInitialDockPending) return;

    // Keep the user's saved docking position when reopening the editor.
    const ImGuiWindowSettings* Settings = ImGui::FindWindowSettingsByID(ImHashStr("Sequencer"));
    if (Settings)
    {
        bInitialDockPending = false;
        return;
    }

    ImGuiWindow* Host = ImGui::FindWindowByName("DockingSpace");
    if (!Host) return;
    const ImGuiID DockSpace = Host->GetID("EngineDockingSpace");
    ImGuiDockNode* Node = ImGui::DockBuilderGetNode(DockSpace);
    if (!Node) return;

    // Use the bottom leaf of the existing layout without rebuilding other panels.
    while (Node->IsSplitNode())
    {
        ImGuiDockNode* A = Node->ChildNodes[0];
        ImGuiDockNode* B = Node->ChildNodes[1];
        Node = B->Pos.y > A->Pos.y ? B : A;
    }
    ImGuiID Remaining = 0;
    const ImGuiID Bottom = ImGui::DockBuilderSplitNode(Node->ID, ImGuiDir_Down, 0.5f, nullptr, &Remaining);
    ImGui::DockBuilderDockWindow("Sequencer", Bottom);
    ImGui::DockBuilderFinish(DockSpace);
    bInitialDockPending = false;
}

void FSequencerPanel::OnRender()
{
    SetInitialDock();
    ImGui::SetNextWindowSize(ImVec2(1000.0f, 280.0f), ImGuiCond_FirstUseEver);
    bool bOpen = IsOpen();
    const bool bVisible = ImGui::Begin(GetPanelName(), &bOpen);
    SetOpen(bOpen);
    if (bVisible)
    {
        DrawToolbar();
        if (!FileError.empty()) ImGui::TextWrapped("%s", FileError.c_str());
        ImGui::Separator();
        DrawTimeline();
        DrawSequenceFileDialog();

        // 숫자나 텍스트를 편집 중일 때는 입력 위젯이 Delete 키를 처리합니다.
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
            ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            DeleteSelectedKey();
    }
    ImGui::End();
    if (!bOpen) ReleasePreview();
    else if (bCurveEditorOpen) DrawCurveEditor();
}

bool FSequencerPanel::SaveSequenceJson(const std::filesystem::path& Path)
{
    const auto Temporary = std::filesystem::path(Path.wstring() + L".tmp");
    try
    {
        const auto SerializeKey = [](const FSequenceKeyframe& Key) {
            json Data = {{"time", Key.Time}, {"value", Key.Value}};
            if (Key.Interpolation) Data["interpolation"] = InterpolationName(*Key.Interpolation);
            return Data;
        };
        json Document = { {"version", 5}, {"duration", PreviewSequence->Duration},
            {"playback_start", Player.GetPlaybackStart()}, {"playback_end", Player.GetPlaybackEnd()},
            {"frames_per_second", Player.GetFramesPerSecond()}, {"tracks", json::array()} };
        for (const auto& Track : EditorTracks)
        {
            if (Track.RuntimeTrackIndex < 0) continue;
            const auto& RuntimeTrack = PreviewSequence->Tracks[Track.RuntimeTrackIndex];
            json Keys = json::array();
            for (const auto& Section : RuntimeTrack.Sections)
                for (const auto& Key : Section.GetKeys())
                {
                    if (Track.Type == 0) Keys.push_back(SerializeKey(Key));
                    else Keys.push_back({{"time", Key.Time}, {"location", Key.Camera.Location},
                        {"rotation", Key.Camera.Rotation}, {"fov", Key.Camera.FieldOfView}});
                }
            json Data = {{"type", Track.Type == 0 ? "Fade" : "Camera"}, {"name", Track.Name}, {"keys", std::move(Keys)}};
            const auto Mode = RuntimeTrack.Sections.empty() ? ESequenceInterpolation::Linear : RuntimeTrack.Sections.front().Interpolation;
            Data["interpolation"] = InterpolationName(Mode);
            if (Track.Type == 1)
            {
                Data["camera_actor"] = RuntimeTrack.CameraActor;
                Data["camera_component"] = RuntimeTrack.CameraComponent;
                const auto& Section = RuntimeTrack.Sections.front();
                Data["defaults"] = {{"location", Section.CameraDefaults.Location},
                    {"rotation", Section.CameraDefaults.Rotation}, {"fov", Section.CameraDefaults.FieldOfView},
                    {"projection", Section.CameraDefaults.Projection}, {"ortho_width", Section.CameraDefaults.OrthoWidth}};
                Data["channels"] = json::array();
                const char* Names[] = {"Location.X", "Location.Y", "Location.Z", "Pitch", "Yaw", "Roll", "FOV", "Projection", "OrthoWidth"};
                for (int Channel = 0; Channel < 9; ++Channel)
                {
                    const auto& Curve = Section.CameraChannels[Channel];
                    json ChannelKeys = json::array();
                    for (const auto& Key : Curve.GetKeys()) ChannelKeys.push_back(SerializeKey(Key));
                    const auto Interp = Curve.Interpolation;
                    Data["channels"].push_back({{"name", Names[Channel]}, {"keys", std::move(ChannelKeys)},
                        {"interpolation", InterpolationName(Interp)}});
                }
            }
            Document["tracks"].push_back(std::move(Data));
        }
        {
            std::ofstream File(Temporary, std::ios::binary | std::ios::trunc);
            File.exceptions(std::ios::failbit | std::ios::badbit);
            File << Document.dump(2) << '\n';
            File.close();
        }
        if (!MoveFileExW(Temporary.c_str(), Path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace the destination file.");
        SequencePath = Path;
        FileError.clear();
        return true;
    }
    catch (const std::exception& Error)
    {
        std::error_code Ignored;
        std::filesystem::remove(Temporary, Ignored);
        FileError = std::string("Save failed: ") + Error.what();
        return false;
    }
}

bool FSequencerPanel::LoadSequenceJson(const std::filesystem::path& Path)
{
    FSequenceAsset Asset;
    if (!LoadSequenceAsset(Path, Asset, FileError))
    {
        FileError = "Open failed: " + FileError;
        return false;
    }
    Player.ReleaseAppliedValues();
    PreviewSequence = std::move(Asset.Sequence);
    EditorTracks.clear();
    for (int Index = 0; Index < static_cast<int>(PreviewSequence->Tracks.size()); ++Index)
    {
        FEditorTrack Track;
        Track.Id = Index * 2 + 1;
        Track.Type = static_cast<int>(PreviewSequence->Tracks[Index].Type);
        Track.RuntimeTrackIndex = Index;
        Track.SectionIds = {Track.Id + 1};
        Track.Name = Asset.TrackNames[Index];
        EditorTracks.push_back(std::move(Track));
    }
    ResetEditorState(Asset.FramesPerSecond);
    NextEditorId = static_cast<int>(EditorTracks.size()) * 2 + 1;
    RebuildItemsFromSequence(EditorTracks.empty() ? 0 : EditorTracks.front().Id, 0);
    Player.SetSequence(PreviewSequence);
    Player.SetPlaybackRange(Asset.PlaybackStart, Asset.PlaybackEnd);
    SequencePath = Path;
    FileError.clear();
    return true;
}

void FSequencerPanel::OpenSequenceDialog(bool bSave)
{
    // OS 모달 창으로 렌더 루프를 막지 않고, ImGui 창에서 경로를 선택합니다.
    std::error_code Error;
    const auto Folder = std::filesystem::absolute("Assets/Sequences", Error);
    if (Error) { FileError = "Could not resolve Assets/Sequences."; return; }
    std::filesystem::create_directories(Folder, Error);
    if (Error) { FileError = "Could not create Assets/Sequences."; return; }

    bSaveSequenceDialog = bSave;
    bConfirmSequenceOverwrite = false;
    const auto FileName = SequencePath.empty() ? std::u8string(u8"Sequence.json") : SequencePath.u8string();
    ImFormatString(SequenceFileName.data(), SequenceFileName.size(), "%s",
        reinterpret_cast<const char*>(FileName.c_str()));
    SequenceFileChoices.clear();
    std::filesystem::directory_iterator It(Folder, Error), End;
    for (; !Error && It != End; It.increment(Error))
    {
        if (It->is_regular_file(Error) && It->path().extension() == L".json")
            SequenceFileChoices.push_back(It->path());
    }
    std::sort(SequenceFileChoices.begin(), SequenceFileChoices.end());
    FileError = Error ? "Could not list sequence files. Enter a file path below." : "";
    ImGui::OpenPopup("Sequence File");
}

void FSequencerPanel::DrawSequenceFileDialog()
{
    ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal("Sequence File", nullptr, ImGuiWindowFlags_NoDocking)) return;

    ImGui::TextUnformatted(bSaveSequenceDialog ? "Save Sequence" : "Open Sequence");
    ImGui::TextDisabled("Folder: Assets/Sequences");
    if (ImGui::BeginChild("SequenceFiles", ImVec2(0, 160), ImGuiChildFlags_Borders))
    {
        for (const auto& Path : SequenceFileChoices)
        {
            const auto Name = Path.filename().u8string();
            if (ImGui::Selectable(reinterpret_cast<const char*>(Name.c_str())))
            {
                const auto FullPath = Path.u8string();
                ImFormatString(SequenceFileName.data(), SequenceFileName.size(), "%s",
                    reinterpret_cast<const char*>(FullPath.c_str()));
                bConfirmSequenceOverwrite = false;
            }
        }
    }
    ImGui::EndChild();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##SequencePath", SequenceFileName.data(), SequenceFileName.size()))
        bConfirmSequenceOverwrite = false;
    ImGui::TextDisabled("File name or full path (.json)");
    if (!FileError.empty()) ImGui::TextWrapped("%s", FileError.c_str());
    if (bConfirmSequenceOverwrite) ImGui::TextUnformatted("File exists. Press Overwrite to replace it.");

    const char* Action = !bSaveSequenceDialog ? "Open" : bConfirmSequenceOverwrite ? "Overwrite" : "Save";
    ImGui::BeginDisabled(SequenceFileName[0] == '\0');
    if (ImGui::Button(Action))
    {
        try
        {
            auto Path = std::filesystem::u8path(SequenceFileName.data());
            if (Path.is_relative()) Path = std::filesystem::path("Assets/Sequences") / Path;
            if (!Path.has_extension()) Path += L".json";
            Path = std::filesystem::absolute(Path).lexically_normal();

            if (bSaveSequenceDialog && !bConfirmSequenceOverwrite && std::filesystem::exists(Path))
                bConfirmSequenceOverwrite = true;
            else if (bSaveSequenceDialog ? SaveSequenceJson(Path) : LoadSequenceJson(Path))
                ImGui::CloseCurrentPopup();
        }
        catch (const std::exception& Error)
        {
            FileError = std::string("Invalid file path: ") + Error.what();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void FSequencerPanel::DrawToolbar()
{
    DrawFileControls();
    NextToolbarControl(65);
    DrawAddTrackButton();
    NextToolbarControl(130);
    if (ImGui::Button("Curve Editor"))
    {
        bCurveEditorOpen = !bCurveEditorOpen;
        CurveTrackId = -1;
        ++CurveEditorSerial;
    }
    DrawTransportBar();
    DrawPlaybackSettings();
    DrawCameraSettings();
    NextToolbarControl(260);
    DrawSelectedKeyInterpolation();
}

void FSequencerPanel::DrawFileControls()
{
    if (ImGui::Button("New")) NewSequence();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("New Sequence (clears unsaved edits)");
    NextToolbarControl(55);
    if (ImGui::Button("Open")) OpenSequenceDialog(false);
    NextToolbarControl(55);
    if (ImGui::Button("Save"))
    {
        if (SequencePath.empty()) OpenSequenceDialog(true);
        else SaveSequenceJson(SequencePath);
    }
    NextToolbarControl(75);
    if (ImGui::Button("Save As")) OpenSequenceDialog(true);
    const auto FileName = SequencePath.empty() ? std::u8string(u8"Untitled") : SequencePath.filename().u8string();
    const char* FileLabel = reinterpret_cast<const char*>(FileName.c_str());
    NextToolbarControl(ImGui::CalcTextSize(FileLabel).x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", FileLabel);
    if (!SequencePath.empty() && ImGui::IsItemHovered())
    {
        const auto FullPath = SequencePath.u8string();
        ImGui::SetTooltip("%s", reinterpret_cast<const char*>(FullPath.c_str()));
    }
}

void FSequencerPanel::DrawAddTrackButton()
{
    if (ImGui::Button("+ Add")) ImGui::OpenPopup("AddSequenceTrack");
    if (!ImGui::BeginPopup("AddSequenceTrack")) return;
    for (int Type = 0; Type < 2; ++Type)
    {
        const bool Exists = std::any_of(EditorTracks.begin(), EditorTracks.end(),
            [Type](const FEditorTrack& Track) { return Track.Type == Type; });
        if (ImGui::MenuItem(Type == 0 ? "Fade" : "Camera", nullptr, false, !Exists)) AddTrack(Type);
    }
    ImGui::EndPopup();
}

void FSequencerPanel::DrawPlaybackSettings()
{
    NextToolbarControl(100.0f + ImGui::CalcTextSize("Time (s)").x + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(100.0f);
    float CurrentTime = Player.GetCurrentTime();
    if (ImGui::DragFloat("Time (s)", &CurrentTime, 0.01f, Player.GetPlaybackStart(), Player.GetPlaybackEnd(), "%.2f"))
    {
        if (!std::isfinite(CurrentTime)) CurrentTime = 0.0f;
        Player.Seek(CurrentTime);
    }
    NextToolbarControl(100.0f + ImGui::CalcTextSize("Length (s)").x + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(100.0f);
    float& Duration = PreviewSequence->Duration;
    const float PreviousDuration = Duration;
    if (ImGui::DragFloat("Length (s)", &Duration, 0.1f, 0.1f, 3600.0f, "%.2f"))
    {
        if (!std::isfinite(Duration)) Duration = 5.0f;
        Duration = std::clamp(Duration, 0.1f, 3600.0f);
        if (Player.GetCurrentTime() > Duration) Player.Seek(Duration);
        const float End = Player.GetPlaybackEnd() == PreviousDuration ? Duration : std::min(Player.GetPlaybackEnd(), Duration);
        const float Start = std::clamp(Player.GetPlaybackStart(), 0.0f, std::max(0.0f, End - 1.0f / Player.GetFramesPerSecond()));
        Player.SetPlaybackRange(Start, End);
    }

    NextToolbarControl(100.0f + ImGui::CalcTextSize("Speed").x + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(100.0f);
    float PlaybackSpeed = Player.GetPlaybackSpeed();
    if (ImGui::DragFloat("Speed", &PlaybackSpeed, 0.05f, 0.01f, 10.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp))
        Player.SetPlaybackSpeed(PlaybackSpeed);
}

std::vector<UCameraComponent*> FSequencerPanel::GetCameraChoices() const
{
    std::vector<UCameraComponent*> Cameras;
    UWorld* World = WorldProvider ? WorldProvider() : nullptr;
    if (!World) return Cameras;
    if (auto* Main = World->GetMainCameraComponent()) Cameras.push_back(Main);
    for (ULevel* Level : World->GetLevels())
    {
        if (!Level) continue;
        for (AActor* Actor : Level->GetActors())
        {
            if (!Actor) continue;
            for (UActorComponent* Component : Actor->GetComponents())
                if (auto* Camera = Cast<UCameraComponent>(Component))
                    if (std::find(Cameras.begin(), Cameras.end(), Camera) == Cameras.end()) Cameras.push_back(Camera);
        }
    }
    return Cameras;
}

UCameraComponent* FSequencerPanel::ResolveCamera(const std::string& Actor, const std::string& Component) const
{
    for (auto* Camera : GetCameraChoices())
    {
        const bool Main = Camera == Camera->GetWorld()->GetMainCameraComponent();
        const std::string ActorName = Main ? "$MainCamera" : Camera->GetOwner()->GetName();
        if (ActorName == Actor && Camera->GetName() == Component) return Camera;
    }
    return nullptr;
}

void FSequencerPanel::ApplyCameraResult(std::optional<FSequenceEvaluatedValue> Result)
{
    if (PilotCameraValue && (!IsCameraPilotEnabled() || Player.GetCurrentTime() != PilotCameraTime))
        PilotCameraValue.reset();
    APlayerCameraManager* Manager = CameraManagerProvider ? CameraManagerProvider() : nullptr;
    UCameraComponent* Camera = Result ? ResolveCamera(Result->CameraActor, Result->CameraComponent) : nullptr;
    bCameraPreviewRequested = Camera != nullptr;
    if (!Camera)
    {
        PilotCameraValue.reset();
        if (Manager) Manager->ReleaseSequenceCamera(&Player);
        if (CameraPreviewCallback) CameraPreviewCallback(std::nullopt);
        return;
    }
    FCameraPOV POV = Camera->GetCameraPOV();
    const auto& Value = Result->Camera;
    POV.Location = FVector(Value.Location[0], Value.Location[1], Value.Location[2]);
    POV.Rotation = FRotator(Value.Rotation[0], Value.Rotation[1], Value.Rotation[2]);
    POV.FieldOfView = Value.FieldOfView;
    POV.PerspectiveBlend = 1.0f - Value.Projection;
    POV.bIsPerspective = POV.PerspectiveBlend >= 0.5f;
    POV.OrthoWidth = Value.OrthoWidth;
    if (PilotCameraValue)
    {
        const auto& Draft = *PilotCameraValue;
        POV.Location = FVector(Draft.Location[0], Draft.Location[1], Draft.Location[2]);
        POV.Rotation = FRotator(Draft.Rotation[0], Draft.Rotation[1], Draft.Rotation[2]);
        POV.FieldOfView = Draft.FieldOfView;
        POV.PerspectiveBlend = 1.0f - Draft.Projection;
        POV.bIsPerspective = POV.PerspectiveBlend >= 0.5f;
        POV.OrthoWidth = Draft.OrthoWidth;
    }
    if (Manager) Manager->SetSequenceCamera(&Player, Camera, POV);
    if (CameraPreviewCallback) CameraPreviewCallback(bCameraPreviewEnabled ? std::optional<FCameraPOV>(POV) : std::nullopt);
}

bool FSequencerPanel::IsCameraPilotEnabled() const
{
    UWorld* World = WorldProvider ? WorldProvider() : nullptr;
    const int Index = GetSelectedTrackIndex();
    return bCameraPilotEnabled && bCameraPreviewEnabled && !Player.IsPlaying() &&
        World && Index >= 0 && EditorTracks[Index].Type == 1 &&
        ResolveCamera(PreviewSequence->Tracks[EditorTracks[Index].RuntimeTrackIndex].CameraActor,
            PreviewSequence->Tracks[EditorTracks[Index].RuntimeTrackIndex].CameraComponent) != nullptr;
}

void FSequencerPanel::OnCameraViewEdited(const FCameraPOV& POV)
{
    if (!IsCameraPilotEnabled() || !POV.IsValid()) return;
    FSequenceCameraValue Value;
    Value.Location = {POV.Location.X, POV.Location.Y, POV.Location.Z};
    Value.Rotation = {POV.Rotation.Pitch, POV.Rotation.Yaw, POV.Rotation.Roll};
    Value.FieldOfView = POV.FieldOfView;
    Value.Projection = 1.0f - POV.PerspectiveBlend;
    Value.OrthoWidth = POV.OrthoWidth;
    PilotCameraValue = Value;
    PilotCameraTime = Player.GetCurrentTime();
    SelectedKeyTime.reset();
}

void FSequencerPanel::TickPIECameraPilot(const FViewportCameraInput& Input, float DeltaTime,
    float MoveSpeed, float MouseSensitivity, float ViewWidth)
{
    if (!IsCameraPilotEnabled()) return;
    const auto& Track = PreviewSequence->Tracks[EditorTracks[GetSelectedTrackIndex()].RuntimeTrackIndex];
    auto* BoundCamera = ResolveCamera(Track.CameraActor, Track.CameraComponent);
    if (!BoundCamera) return;

    const auto Value = PilotCameraValue && PilotCameraTime == Player.GetCurrentTime()
        ? *PilotCameraValue : GetCameraKeyValue(Player.GetCurrentTime());
    FViewportCameraState Camera{
        FVector(Value.Location[0], Value.Location[1], Value.Location[2]),
        FRotator(Value.Rotation[0], Value.Rotation[1], Value.Rotation[2]),
        Value.OrthoWidth, std::max(1.0f, ViewWidth), Value.Projection <= 0.5f};
    ApplyViewportCameraMovement(Camera, Input, DeltaTime, MoveSpeed, MouseSensitivity);

    FCameraPOV POV = BoundCamera->GetCameraPOV();
    POV.Location = Camera.Location;
    POV.Rotation = Camera.Rotation;
    POV.FieldOfView = Value.FieldOfView;
    POV.bIsPerspective = Camera.bPerspective;
    POV.PerspectiveBlend = 1.0f - Value.Projection;
    POV.OrthoWidth = Camera.OrthoWidth;
    OnCameraViewEdited(POV);
    Player.Apply();
}

FSequenceCameraValue FSequencerPanel::GetCameraKeyValue(float Time) const
{
    FSequenceCameraValue Value;
    const int Index = GetSelectedTrackIndex();
    if (Index < 0) return Value;
    const auto& Track = PreviewSequence->Tracks[EditorTracks[Index].RuntimeTrackIndex];
    if (!Track.Sections.empty())
    {
        Value = Track.Sections.front().CameraDefaults;
        if (Track.Sections.front().EvaluateCamera(Time, Value)) return Value;
    }
    if (auto* Camera = ResolveCamera(Track.CameraActor, Track.CameraComponent))
    {
        const auto POV = Camera->GetCameraPOV();
        Value.Location = {POV.Location.X, POV.Location.Y, POV.Location.Z};
        Value.Rotation = {POV.Rotation.Pitch, POV.Rotation.Yaw, POV.Rotation.Roll};
        Value.FieldOfView = POV.FieldOfView;
        Value.Projection = 1.0f - POV.PerspectiveBlend;
        Value.OrthoWidth = POV.OrthoWidth;
    }
    return Value;
}

void FSequencerPanel::EditSelectedCameraKey(float Time, const FSequenceCameraValue& Value)
{
    auto* Section = GetSelectedSection();
    if (!Section || !std::isfinite(Time) || !Value.IsValid()) return;
    Time = std::clamp(Time, 0.0f, PreviewSequence->Duration);
    for (int Channel : GetRowChannels(SelectedTrack))
    {
        if (SelectedKeyTime && *SelectedKeyTime != Time &&
            Section->CameraChannels[Channel].EditKey(*SelectedKeyTime, Time, Value.GetChannelValue(Channel))) continue;
        Section->CameraChannels[Channel].AddKey(Time, Value.GetChannelValue(Channel));
    }
    SelectedKeyTime = Time;
    NotifySequenceEdited();
}
void FSequencerPanel::DrawCameraSettings()
{
    const int Index = GetSelectedTrackIndex();
    if (Index < 0 || EditorTracks[Index].Type != 1) return;
    auto& Track = PreviewSequence->Tracks[EditorTracks[Index].RuntimeTrackIndex];
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Camera");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(280.0f, ImGui::GetContentRegionAvail().x));
    const std::string Label = Track.CameraActor.empty() ? "Select camera" :
        Track.CameraActor == "$MainCamera" ? "Main Camera" : Track.CameraActor;
    if (ImGui::BeginCombo("##CameraBinding", Label.c_str()))
    {
        if (ImGui::Selectable("None", Track.CameraActor.empty()))
        {
            Track.CameraActor.clear(); Track.CameraComponent.clear(); NotifySequenceEdited();
        }
        for (auto* Camera : GetCameraChoices())
        {
            const bool Main = Camera == Camera->GetWorld()->GetMainCameraComponent();
            const std::string Actor = Main ? "$MainCamera" : Camera->GetOwner()->GetName();
            const std::string Component = Camera->GetName();
            const std::string Name = Actor + " / " + Component;
            if (ImGui::Selectable(Name.c_str(), Actor == Track.CameraActor && Component == Track.CameraComponent))
            {
                Track.CameraActor = Actor; Track.CameraComponent = Component;
                const auto POV = Camera->GetCameraPOV();
                auto& Defaults = Track.Sections.front().CameraDefaults;
                Defaults.Location = {POV.Location.X, POV.Location.Y, POV.Location.Z};
                Defaults.Rotation = {POV.Rotation.Pitch, POV.Rotation.Yaw, POV.Rotation.Roll};
                Defaults.FieldOfView = POV.FieldOfView;
                Defaults.Projection = 1.0f - POV.PerspectiveBlend;
                Defaults.OrthoWidth = POV.OrthoWidth;
                NotifySequenceEdited();
            }
        }
        ImGui::EndCombo();
    }
    const bool Bound = ResolveCamera(Track.CameraActor, Track.CameraComponent) != nullptr;
    NextToolbarControl(130);
    if (ImGui::Checkbox("Preview", &bCameraPreviewEnabled)) NotifySequenceEdited();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable camera preview and Pilot to frame shots in the editor or PIE.");
    UWorld* World = WorldProvider ? WorldProvider() : nullptr;
    const bool CanPilot = Bound && bCameraPreviewEnabled && World;
    NextToolbarControl(130);
    ImGui::BeginDisabled(!CanPilot);
    if (ImGui::Checkbox("Pilot", &bCameraPilotEnabled))
    {
        PilotCameraValue.reset();
        if (bCameraPilotEnabled)
        {
            Player.Pause();
            if (!GetSelectedSection()->HasCameraKeys()) AddKey(Player.GetCurrentTime());
        }
        Player.Apply();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(CanPilot ? "Hold RMB and use WASD/QE to move, then Capture the complete camera pose." :
            "Pilot requires a bound camera and Preview enabled. Pause Sequencer to move the camera.");
    ImGui::EndDisabled();
    if (!Bound) ImGui::TextDisabled("Camera not found in current world. Select a camera to preview.");
    ImGui::SameLine();
    ImGui::BeginDisabled(!Bound);
    if (ImGui::Button("+ Key")) AddKey(Player.GetCurrentTime());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Record the current evaluated or piloted camera pose.");
    ImGui::SameLine();
    if (ImGui::Button("Capture"))
    {
        auto* Camera = ResolveCamera(Track.CameraActor, Track.CameraComponent);
        const auto POV = Camera->GetCameraPOV();
        FSequenceCameraValue Value;
        Value.Location = {POV.Location.X, POV.Location.Y, POV.Location.Z};
        Value.Rotation = {POV.Rotation.Pitch, POV.Rotation.Yaw, POV.Rotation.Roll};
        Value.FieldOfView = POV.FieldOfView;
        Value.Projection = 1.0f - POV.PerspectiveBlend;
        Value.OrthoWidth = POV.OrthoWidth;
        SelectedKeyTime = Player.GetCurrentTime();
        if (IsCameraPilotEnabled())
            Value = PilotCameraValue && PilotCameraTime == Player.GetCurrentTime()
                ? *PilotCameraValue : GetCameraKeyValue(Player.GetCurrentTime());
        GetSelectedSection()->AddCameraKey(Player.GetCurrentTime(), Value);
        NotifySequenceEdited();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Record the complete piloted pose, or the bound camera when not piloting.");
    ImGui::EndDisabled();

    NextToolbarControl(200);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Projection Blend");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    auto& ProjectionInterpolation = Track.Sections.front().CameraChannels[7].Interpolation;
    int Mode = static_cast<int>(ProjectionInterpolation);
    if (ImGui::Combo("##ProjectionBlend", &Mode, "Linear\0Ease In Out\0Step\0Ease In\0Ease Out\0"))
    {
        ProjectionInterpolation = static_cast<ESequenceInterpolation>(Mode);
        NotifySequenceEdited();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Projection keys: 0 = perspective, 1 = orthographic. Blend between key times.");
}
void FSequencerPanel::DrawContextMenu()
{
    if (!ImGui::BeginPopup("SequencerContext")) return;
    int TrackIndex = -1;
    for (int Index = 0; Index < static_cast<int>(EditorTracks.size()); ++Index)
        if (EditorTracks[Index].Id == ContextTrackId) TrackIndex = Index;
    int Action = 0;
    if (TrackIndex >= 0)
    {
        if (ImGui::MenuItem("Add Key")) Action = 1;
        if (SelectedKeyTime)
        {
            DrawSelectedKeyInterpolation();
            auto* Section = GetSelectedSection();
            if (Section)
            {
                const auto Keys = GetRowKeys(SelectedTrack);
                const auto Found = std::find_if(Keys.begin(), Keys.end(),
                    [this](const FSequenceKeyframe& Key) { return Key.Time == *SelectedKeyTime; });
                if (Found != Keys.end())
                {
                    float Time = Found->Time, Value = Found->Value;
                    ImGui::SetNextItemWidth(100);
                    bool Changed = ImGui::DragFloat("Key (s)", &Time, 0.01f, 0, PreviewSequence->Duration, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SetNextItemWidth(100);
                    if (EditorTracks[TrackIndex].Type == 0)
                        Changed |= ImGui::DragFloat("Value", &Value, 0.01f, 0, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    else if (PreviewData.Items[SelectedTrack].Channel == 6)
                        Changed |= ImGui::DragFloat("Value", &Value, 0.1f, 1, 179, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    else if (PreviewData.Items[SelectedTrack].Channel >= 0)
                        Changed |= ImGui::DragFloat("Value", &Value, 0.05f, 0, 0, "%.3f");
                    if (Changed) EditSelectedKey(Time, Value);
                }
            }
            if (ImGui::MenuItem("Delete Key")) Action = 2;
        }
        if (!bTimelineContext)
        {
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Track")) Action = 3;
        }
    }
    ImGui::EndPopup();
    if (Action == 1) AddKey(bTimelineContext ? ContextTime : Player.GetCurrentTime());
    if (Action == 2) DeleteSelectedKey();
    if (Action == 3) DeleteTrack(TrackIndex);
}

void FSequencerPanel::DrawTransportBar()
{
    const float Duration = PreviewSequence->Duration;
    bool FirstButton = true;
    const auto Button = [&FirstButton](const char* Id, const char* Tooltip, ETransportIcon Icon, bool Active = false)
    {
        if (!FirstButton) NextToolbarControl(ImGui::GetFrameHeight());
        FirstButton = false;
        return TransportButton(Id, Tooltip, Icon, Active);
    };
    if (Button("##RangeStart", "Set playback start", ETransportIcon::RangeStart))
        Player.SetPlaybackRange(std::clamp(Player.GetCurrentTime(), 0.0f,
            std::max(0.0f, Player.GetPlaybackEnd() - 1.0f / Player.GetFramesPerSecond())), Player.GetPlaybackEnd());
    if (Button("##First", "First", ETransportIcon::First))
    {
        Player.Stop();
    }
    if (Button("##PreviousFrame", "Previous frame", ETransportIcon::PreviousFrame))
    {
        Player.StepBackward();
    }
    const bool ReversePlaying = Player.IsPlaying() && Player.GetPlaybackDirection() < 0;
    if (Button("##Reverse", ReversePlaying ? "Pause" : "Play reverse",
        ReversePlaying ? ETransportIcon::Pause : ETransportIcon::Reverse, ReversePlaying))
    {
        if (ReversePlaying) Player.Pause();
        else Player.PlayReverse();
    }
    const bool ForwardPlaying = Player.IsPlaying() && Player.GetPlaybackDirection() > 0;
    if (Button("##Play", ForwardPlaying ? "Pause" : "Play",
        ForwardPlaying ? ETransportIcon::Pause : ETransportIcon::Play, ForwardPlaying))
    {
        if (ForwardPlaying) Player.Pause();
        else Player.Play();
    }
    if (Button("##NextFrame", "Next frame", ETransportIcon::NextFrame))
    {
        Player.StepForward();
    }
    if (Button("##Last", "Last", ETransportIcon::Last))
    {
        Player.Seek(Player.GetPlaybackEnd());
    }
    if (Button("##RangeEnd", "Set playback end", ETransportIcon::RangeEnd))
        Player.SetPlaybackRange(Player.GetPlaybackStart(),
            std::clamp(Player.GetCurrentTime(), Player.GetPlaybackStart() + 1.0f / Player.GetFramesPerSecond(), Duration));
    if (Button("##Loop", Player.IsLooping() ? "Loop playback" : "Play once",
        Player.IsLooping() ? ETransportIcon::Loop : ETransportIcon::Once, Player.IsLooping()))
        Player.SetLoop(!Player.IsLooping());
}

void FSequencerPreviewData::Get(int Index, int** Start, int** End, int* Type, unsigned int* Color)
{
    auto& Item = Items[Index];
    if (Start) *Start = &Item.Start;
    if (End) *End = &Item.End;
    if (Type) *Type = Item.Type;
    if (Color) *Color = Item.SectionIndex < 0 ? 0 : IM_COL32(95, 165, 220, 255);
}

void FSequencerPanel::DrawTimeline()
{
    const float Duration = PreviewSequence->Duration;
    PreviewData.FirstFrame = GetTimelineFirstFrame(Player.GetFramesPerSecond());
    PreviewData.LastFrame = std::max(1, static_cast<int>(std::lround(Duration * Player.GetFramesPerSecond())));
    FirstVisibleFrame = std::clamp(FirstVisibleFrame, PreviewData.GetFrameMin(), PreviewData.LastFrame - 1);
    const float Height = std::max(1.0f, ImGui::GetContentRegionAvail().y);
    // Small docks scroll the canvas rather than hiding it or giving the widget negative sizes.
    if (ImGui::BeginChild("Timeline", ImVec2(0, Height), ImGuiChildFlags_Borders,
        ImGuiWindowFlags_HorizontalScrollbar))
    {
        const ImVec2 Available = ImGui::GetContentRegionAvail();
        const ImVec2 CanvasSize(std::max(620.0f, Available.x), std::max(150.0f, Available.y));
        if (ImGui::BeginChild("SequencerCanvas", CanvasSize, ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            const ImVec2 CanvasOrigin = ImGui::GetCursorScreenPos();
            const float Width = ImGui::GetContentRegionAvail().x;
            const float CanvasHeight = ImGui::GetContentRegionAvail().y;
            TrackListWidth = std::clamp(TrackListWidth, 240.0f, Width - 160.0f);
            const ImVec2 Mouse = ImGui::GetIO().MousePos;
            const ImRect Splitter(ImVec2(CanvasOrigin.x + TrackListWidth - 3, CanvasOrigin.y),
                ImVec2(CanvasOrigin.x + TrackListWidth + 3, CanvasOrigin.y + CanvasHeight));
            const bool Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            if (Hovered && Splitter.Contains(Mouse))
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                if (ImGui::IsMouseClicked(0)) bDraggingTrackBoundary = true;
            }
            if (!ImGui::IsMouseDown(0)) bDraggingTrackBoundary = false;
            if (bDraggingTrackBoundary)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                TrackListWidth = std::clamp(Mouse.x - CanvasOrigin.x, 240.0f, Width - 160.0f);
            }

            // ImSequencer's fixed 200px legend is translated to our adjustable boundary.
            // The panel draws its labels in the resulting space; vendor sources stay untouched.
            const ImVec2 Origin(CanvasOrigin.x, CanvasOrigin.y + TimelineMarkerRowHeight);
            const ImVec2 Size(Width, CanvasHeight - TimelineMarkerRowHeight);
            const float WidgetX = CanvasOrigin.x + TrackListWidth - 200.0f;
            PreviewData.bHasGeometry = false;
            PreviewData.Rows.assign(PreviewData.Items.size(), {});
            PreviewData.PendingScroll = Hovered && Mouse.x >= CanvasOrigin.x && Mouse.x < WidgetX &&
                Mouse.y >= Origin.y + 20 && Mouse.y < Origin.y + Size.y - 22 ? ImGui::GetIO().MouseWheel : 0.0f;
            if (bExpanded && !PreviewData.Items.empty())
            {
                ImGui::SetCursorScreenPos(ImVec2(WidgetX, Origin.y));
                ImGui::PushFont(ImGui::GetFont(), std::min(ImGui::GetFontSize(), 16.0f));
                ImSequencer::Sequencer(&PreviewData, nullptr, nullptr, &SelectedTrack,
                    &FirstVisibleFrame, ImSequencer::SEQUENCER_EDIT_NONE);
                ImGui::PopFont();
            }
            SyncSequence();
            if (PreviewData.bHasGeometry)
            {
                TimeTransform.PixelsPerSecond = PreviewData.PixelsPerFrame * Player.GetFramesPerSecond();
                TimeTransform.TimelineX = PreviewData.TimelineLeft;
                TimeTransform.VisibleStartTime = (PreviewData.TimelineLeft - PreviewData.ZeroFrameX) / TimeTransform.PixelsPerSecond;
            }
            ImGui::PushFont(ImGui::GetFont(), std::min(ImGui::GetFontSize(), 16.0f));
            DrawTrackList(Origin, Size);
            ImGui::PopFont();
            if (bExpanded && PreviewData.bHasGeometry)
            {
                DrawKeys();
                if (!bDraggingTrackBoundary) DrawTimelineOverlay(Origin, Size);
            }
            ImGui::GetWindowDrawList()->AddLine(ImVec2(CanvasOrigin.x + TrackListWidth, CanvasOrigin.y),
                ImVec2(CanvasOrigin.x + TrackListWidth, CanvasOrigin.y + CanvasHeight),
                ImGui::GetColorU32(bDraggingTrackBoundary ? ImGuiCol_SeparatorActive : ImGuiCol_Separator), 2.0f);
            DrawContextMenu();
            SyncSequence();
        }
        ImGui::EndChild(); // SequencerCanvas reserves the full scrollable extent.
    }
    ImGui::EndChild();
    SyncSequence();
}

void FSequencerPreviewData::CustomDraw(int Index, ImDrawList* DrawList,
    const ImRect& Rect, const ImRect&, const ImRect& Clip, const ImRect&)
{
    ImRect FullRow = Rect;
    FullRow.Min.y -= 20.0f;
    ImRect FullClip = Clip;
    FullClip.Min.y -= 20.0f;
    CustomDrawCompact(Index, DrawList, FullRow, FullClip);
}

void FSequencerPreviewData::CustomDrawCompact(int Index, ImDrawList* /*DrawList*/,
    const ImRect& Rect, const ImRect& Clip)
{
    PixelsPerFrame = Rect.GetWidth() / float(GetFrameMax() - GetFrameMin() + 1);
    ZeroFrameX = Rect.Min.x - (GetFrameMin() + 0.5f) * PixelsPerFrame;
    TimelineLeft = Clip.Min.x;
    TimelineRight = Clip.Max.x;
    bHasGeometry = PixelsPerFrame > 0.0f;
    if (Index >= 0 && Index < static_cast<int>(Rows.size()))
    {
        const ImVec2 ChildPos = ImGui::GetWindowPos();
        const float ChildBottom = ChildPos.y + ImGui::GetWindowSize().y;
        Rows[Index] = { Rect.Min.y, Rect.Max.y, std::max(Rect.Min.y, ChildPos.y), std::min(Rect.Max.y, ChildBottom) };
    }
    if (Index == 0 && PendingScroll != 0.0f)
    {
        ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - PendingScroll * ImGui::GetTextLineHeight() * 3));
        PendingScroll = 0.0f;
    }
}

void FSequencerPanel::DrawTrackList(const ImVec2& Origin, const ImVec2& Size)
{
    ImDrawList* Draw = ImGui::GetWindowDrawList();
    const float Left = Origin.x;
    const float Right = Left + TrackListWidth;
    const float Top = Origin.y - TimelineMarkerRowHeight;
    const float Bottom = Origin.y + Size.y - 22;
    const float NameRight = Right - 166;
    bool RebuildRows = false;
    Draw->PushClipRect(ImVec2(Left, Top), ImVec2(Right, Bottom), true);
    Draw->AddRectFilled(ImVec2(Left, Top), ImVec2(Right, Bottom), IM_COL32(30, 30, 32, 255));
    Draw->AddText(ImVec2(Left + 10, Top + 4), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Tracks");
    if (bExpanded)
    {
        for (int Index = 0; Index < PreviewData.GetItemCount() && Index < static_cast<int>(PreviewData.Rows.size()); ++Index)
        {
            const auto& Row = PreviewData.Rows[Index];
            const float ClipTop = std::max(Origin.y + 20, Row.ClipTop);
            const float ClipBottom = std::min(Bottom, Row.ClipBottom);
            if (ClipBottom <= ClipTop) continue;
            Draw->PushClipRect(ImVec2(Left, ClipTop), ImVec2(NameRight, ClipBottom), true);
            const ImRect Hit(ImVec2(Left, ClipTop), ImVec2(NameRight, ClipBottom));
            const bool Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && Hit.Contains(ImGui::GetIO().MousePos);
            if (Hovered && !bDraggingTrackBoundary && ImGui::IsMouseClicked(0) && SelectedTrack != Index)
            {
                SelectedKeyTime.reset(); SelectedTrack = Index;
            }
            const auto& Item = PreviewData.Items[Index];
            const ImU32 Background = SelectedTrack == Index ? IM_COL32(50, 59, 68, 255) :
                Hovered ? IM_COL32(42, 43, 47, 255) : IM_COL32(26, 27, 30, 255);
            Draw->AddRectFilled(ImVec2(Left, Row.Top), ImVec2(NameRight, Row.Bottom), Background);
            const float CenterY = (Row.Top + Row.Bottom) * 0.5f;
            float LabelX = Left + 12;
            if (Item.Type == 1)
            {
                const bool Group = Item.Group != 0;
                const float Indent = Item.Group == 3 ? 0.0f :
                    Item.Channel >= 0 && Item.Channel < 6 ? 40.0f : 20.0f;
                LabelX += Indent;
                if (Group)
                {
                    bool& Expanded = Item.Group == 3 ? bCameraExpanded :
                        Item.Group == 1 ? bCameraLocationExpanded : bCameraRotationExpanded;
                    const float X = LabelX + 5;
                    if (Expanded) Draw->AddTriangleFilled(ImVec2(X - 4, CenterY - 2), ImVec2(X + 4, CenterY - 2), ImVec2(X, CenterY + 3), IM_COL32(145, 148, 155, 255));
                    else Draw->AddTriangleFilled(ImVec2(X - 2, CenterY - 4), ImVec2(X - 2, CenterY + 4), ImVec2(X + 3, CenterY), IM_COL32(145, 148, 155, 255));
                    if (Hovered && ImGui::IsMouseClicked(0) && ImGui::GetIO().MousePos.x < LabelX + 16)
                    { Expanded = !Expanded; RebuildRows = true; }
                    LabelX += 20;
                }
                if (Hovered && ImGui::IsMouseClicked(0) && Item.Channel >= 0)
                { CurveChannel = Item.Channel; CurveTrackId = -1; }
            }
            Draw->AddText(ImVec2(LabelX, CenterY - ImGui::GetTextLineHeight() * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), Item.Name.c_str());
            Draw->AddLine(ImVec2(Left, Row.Bottom - 1), ImVec2(Right, Row.Bottom - 1), IM_COL32(12, 13, 15, 255));
            const ImVec2 Mouse = ImGui::GetIO().MousePos;
            const bool OnSection = Item.SectionIndex >= 0 && Mouse.y >= ClipTop && Mouse.y < ClipBottom &&
                Mouse.x >= std::max(Right, TimeTransform.TimeToX(GetSections(Item.TrackIndex)[Item.SectionIndex].StartTime)) &&
                Mouse.x <= std::min(PreviewData.TimelineRight, TimeTransform.TimeToX(GetSections(Item.TrackIndex)[Item.SectionIndex].EndTime) + PreviewData.PixelsPerFrame);
            const bool WindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            if (WindowHovered && (Hovered || OnSection) && ImGui::IsMouseClicked(1))
            {
                if (SelectedTrack != Index) SelectedKeyTime.reset();
                SelectedTrack = Index;
                ContextTrackId = Item.TrackId;
                bTimelineContext = OnSection;
                ContextTime = std::clamp(TimeTransform.XToTime(Mouse.x), 0.0f, PreviewSequence->Duration);
                ImGui::OpenPopup("SequencerContext");
            }
            if (Hovered) ImGui::SetTooltip("%s [%s]", Item.Name.c_str(), PreviewData.GetItemTypeName(Item.Type));
            Draw->PopClipRect();
            DrawTrackKeyControls(Index, NameRight + 4, Right - 6);
        }
    }
    Draw->PopClipRect();
    if (RebuildRows) RebuildItemsFromSequence();
}

void FSequencerPanel::SetCurrentFadeValue(float Value)
{
    auto* Section = GetSelectedSection();
    if (!Section || !std::isfinite(Value)) return;
    const float Time = Player.GetCurrentTime();
    Section->AddKey(Time, std::clamp(Value, 0.0f, 1.0f));
    SelectedKeyTime = Time;
    NotifySequenceEdited();
}

std::optional<float> FSequencerPanel::FindAdjacentKey(int Direction)
{
    auto* Section = GetSelectedSection();
    if (!Section) return std::nullopt;
    std::optional<float> Target;
    for (const auto& Key : GetRowKeys(SelectedTrack))
    {
        if (Key.Time < Player.GetPlaybackStart() || Key.Time > Player.GetPlaybackEnd()) continue;
        if (Direction < 0 && Key.Time < Player.GetCurrentTime()) Target = Key.Time;
        if (Direction > 0 && Key.Time > Player.GetCurrentTime()) return Key.Time;
    }
    return Target;
}

void FSequencerPanel::JumpToKey(int Direction)
{
    const auto Time = FindAdjacentKey(Direction);
    if (!Time) return;
    Player.Pause();
    Player.Seek(*Time);
    SelectedKeyTime = *Time;
}

void FSequencerPanel::DrawTrackKeyControls(int RowIndex, float Left, float Right)
{
    const auto Row = PreviewData.Rows[RowIndex];
    const ImVec2 Restore = ImGui::GetCursorScreenPos();
    ImGui::PushID(PreviewData.Items[RowIndex].Id);
    ImGui::SetCursorScreenPos(ImVec2(Left, Row.ClipTop));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool Visible = ImGui::BeginChild("KeyControls", ImVec2(Right - Left, Row.ClipBottom - Row.ClipTop),
        ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (Visible)
    {
        ImGui::PushClipRect(ImVec2(Left, Row.ClipTop), ImVec2(Right, Row.ClipBottom), true);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 3));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.24f, 0.27f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.32f, 0.37f, 1));
        ImGui::SetCursorScreenPos(ImVec2(Left, Row.Top + (Row.Bottom - Row.Top - 26) * 0.5f));
        float Value = 0;
        const auto& Item = PreviewData.Items[RowIndex];
        auto& Sections = GetSections(Item.TrackIndex);
        auto* Section = Sections.empty() ? nullptr : &Sections.front();
        if (Section) Section->Evaluate(Player.GetCurrentTime(), Value);
        ImGui::SetNextItemWidth(78);
        if (Item.Type == 1 && Item.Channel >= 0)
        {
            FSequenceCameraValue Pose;
            if (Section) Section->EvaluateCamera(Player.GetCurrentTime(), Pose);
            if (PilotCameraValue && PilotCameraTime == Player.GetCurrentTime()) Pose = *PilotCameraValue;
            float* Component = Item.Channel < 3 ? &Pose.Location[Item.Channel] :
                Item.Channel < 6 ? &Pose.Rotation[Item.Channel - 3] :
                Item.Channel == 6 ? &Pose.FieldOfView : Item.Channel == 7 ? &Pose.Projection : &Pose.OrthoWidth;
            const bool Lens = Item.Channel == 6;
            bool Changed = false;
            if (Item.Channel == 7)
            {
                char Label[32];
                if (Pose.Projection == 0.0f) ImFormatString(Label, sizeof(Label), "0: Persp");
                else if (Pose.Projection == 1.0f) ImFormatString(Label, sizeof(Label), "1: Ortho");
                else ImFormatString(Label, sizeof(Label), "%.3f", Pose.Projection);
                if (ImGui::BeginCombo("##Projection", Label))
                {
                    for (int Projection = 0; Projection < 2; ++Projection)
                        if (ImGui::Selectable(Projection == 0 ? "0: Perspective" : "1: Orthographic",
                            Pose.Projection == static_cast<float>(Projection)))
                        {
                            Pose.Projection = static_cast<float>(Projection);
                            Changed = true;
                        }
                    ImGui::EndCombo();
                }
            }
            else
                Changed = ImGui::DragFloat("##CameraValue", Component, Lens ? 0.1f : 0.05f,
                    Lens ? 1.0f : Item.Channel == 8 ? 0.1f : 0.0f,
                    Lens ? 179.0f : Item.Channel == 8 ? 100000.0f : 0.0f, "%.3f");
            if (Changed)
            {
                SelectedTrack = RowIndex;
                SelectedKeyTime = Player.GetCurrentTime();
                EditSelectedCameraKey(Player.GetCurrentTime(), Pose);
                CurveChannel = Item.Channel; CurveTrackId = -1;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Drag to edit %s; Ctrl+click to type a value.", Item.Name.c_str());
        }
        else if (Item.Type == 1)
            ImGui::Dummy(ImVec2(78, 26));
        else if (ImGui::DragFloat("##FadeValue", &Value, 0.01f, 0, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp))
        {
            if (SelectedTrack != RowIndex) SelectedKeyTime.reset();
            SelectedTrack = RowIndex;
            SetCurrentFadeValue(Value);
        }
        bool HasKey = false;
        if (Section)
            for (const auto& Key : GetRowKeys(RowIndex)) if (Key.Time == Player.GetCurrentTime()) HasKey = true;
        const auto KeyButton = [&](const char* Id, int Direction, bool Enabled, const char* Tooltip) {
            ImGui::SameLine();
            ImGui::BeginDisabled(!Enabled);
            const bool Clicked = ImGui::Button(Id, ImVec2(22, 26));
            const auto Min = ImGui::GetItemRectMin();
            const float X = Min.x + 11, Y = Min.y + 13;
            ImDrawList* Draw = ImGui::GetWindowDrawList();
            const ImU32 Color = ImGui::GetColorU32(Direction == 0 && HasKey ? ImVec4(1, 0.8f, 0.25f, 1) : ImGui::GetStyleColorVec4(ImGuiCol_Text));
            if (Direction == 0)
            {
                Draw->AddQuad(ImVec2(X, Y-5), ImVec2(X+5, Y), ImVec2(X, Y+5), ImVec2(X-5, Y), Color, 1.5f);
                Draw->AddLine(ImVec2(X-2,Y),ImVec2(X+2,Y),Color);
                Draw->AddLine(ImVec2(X,Y-2),ImVec2(X,Y+2),Color);
            }
            else
            {
                const float D = float(Direction);
                Draw->AddLine(ImVec2(X-D*2,Y-5),ImVec2(X+D*3,Y),Color,1.5f);
                Draw->AddLine(ImVec2(X+D*3,Y),ImVec2(X-D*2,Y+5),Color,1.5f);
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", Tooltip);
            return Clicked;
        };
        bool HasPrevious = false, HasNext = false;
        if (Section) for (const auto& Key : GetRowKeys(RowIndex))
        {
            if (Key.Time < Player.GetPlaybackStart() || Key.Time > Player.GetPlaybackEnd()) continue;
            HasPrevious |= Key.Time < Player.GetCurrentTime(); HasNext |= Key.Time > Player.GetCurrentTime();
        }
        const auto SelectRow = [&]() {
            if (SelectedTrack != RowIndex) SelectedKeyTime.reset();
            SelectedTrack = RowIndex;
            if (Item.Type == 1 && Item.Channel >= 0 && CurveChannel != Item.Channel)
            { CurveChannel = Item.Channel; CurveTrackId = -1; }
        };
        if (KeyButton("##PreviousKey", -1, HasPrevious, "Previous key")) { SelectRow(); JumpToKey(-1); }
        if (KeyButton("##AddKey", 0, Section != nullptr, "Add or select key at current time")) { SelectRow(); AddKey(Player.GetCurrentTime()); }
        if (KeyButton("##NextKey", 1, HasNext, "Next key")) { SelectRow(); JumpToKey(1); }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        ImGui::PopClipRect();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopID();
    ImGui::SetCursorScreenPos(Restore);
    ImGui::Dummy(ImVec2(0, 0));
}

void FSequencerPanel::DrawKeys()
{
    if (!ImGui::IsMouseDown(0)) bDraggingKey = false;
    for (int Index = 0; Index < PreviewData.GetItemCount() && Index < static_cast<int>(PreviewData.Rows.size()); ++Index)
        DrawTrackKeys(Index);
    auto* Section = GetSelectedSection();
    if (!Section) return;
    const auto Keys = GetRowKeys(SelectedTrack);
    const ImVec2 Mouse = ImGui::GetIO().MousePos;
    if (bDraggingKey && SelectedKeyTime)
    {
        const auto Found = std::find_if(Keys.begin(), Keys.end(),
            [this](const FSequenceKeyframe& Key) { return Key.Time == *SelectedKeyTime; });
        if (Found != Keys.end())
        {
            const float FPS = Player.GetFramesPerSecond();
            const float Delta = std::round((Mouse.x - KeyDragStartX) / TimeTransform.PixelsPerSecond * FPS) / FPS;
            const float Time = std::clamp(KeyDragStartTime + Delta, 0.0f, PreviewSequence->Duration);
            if (Time != *SelectedKeyTime) EditSelectedKey(Time, Found->Value);
        }
    }
}

void FSequencerPanel::DrawTrackKeys(int RowIndex)
{
    const auto& Item = PreviewData.Items[RowIndex];
    auto& Sections = GetSections(Item.TrackIndex);
    if (Item.SectionIndex < 0 || Item.SectionIndex >= static_cast<int>(Sections.size())) return;
    auto* Section = &Sections[Item.SectionIndex];
    const auto Row = PreviewData.Rows[RowIndex];
    if (Row.ClipBottom <= Row.ClipTop) return;
    const auto Keys = GetRowKeys(RowIndex);
    ImDrawList* Draw = ImGui::GetWindowDrawList();
    Draw->PushClipRect(ImVec2(PreviewData.TimelineLeft, Row.ClipTop),
        ImVec2(PreviewData.TimelineRight, Row.ClipBottom), true);
    // Replace the entire vendor row, including its section and selection colors.
    Draw->AddRectFilled(ImVec2(PreviewData.TimelineLeft, Row.Top),
        ImVec2(PreviewData.TimelineRight, Row.Bottom), IM_COL32(36, 36, 36, 255));
    const float Y = (Row.Top + Row.Bottom) * 0.5f;
    // Show the evaluated Fade as a grayscale strip: 0 = gray, 1 = black.
    const float Start = std::max(0.0f, TimeTransform.XToTime(PreviewData.TimelineLeft));
    const float End = std::min(PreviewSequence->Duration, TimeTransform.XToTime(PreviewData.TimelineRight));
    const auto Shade = [&](float Time) {
        if (Item.Type == 1) return Item.Channel >= 0 ? IM_COL32(28, 29, 32, 255) : IM_COL32(48, 43, 57, 255);
        float Value = 0;
        Section->Evaluate(Time, Value);
        const int Gray = static_cast<int>(76 * (1.0f - std::clamp(Value, 0.0f, 1.0f)));
        return IM_COL32(Gray, Gray, Gray, 255);
    };
    const auto Segment = [&](float A, float B) {
        if (B <= A) return;
        auto Mode = Section->Interpolation;
        for (const auto& Key : Keys)
            if (Key.Time <= A) Mode = Key.Interpolation.value_or(Section->Interpolation);
            else break;
        const int Steps = Mode == ESequenceInterpolation::Linear || Mode == ESequenceInterpolation::Constant ? 1 : 20;
        for (int I = 0; I < Steps; ++I)
        {
            const float T0 = A + (B - A) * I / Steps;
            const float T1 = A + (B - A) * (I + 1) / Steps;
            const ImU32 L = Shade(T0), R = Mode == ESequenceInterpolation::Constant ? L : Shade(T1);
            Draw->AddRectFilledMultiColor(ImVec2(TimeTransform.TimeToX(T0), Row.Top),
                ImVec2(TimeTransform.TimeToX(T1), Row.Bottom), L, R, R, L);
        }
    };
    float Previous = Start;
    for (const auto& Key : Keys)
    {
        if (Key.Time <= Start || Key.Time >= End) continue;
        Segment(Previous, Key.Time);
        Previous = Key.Time;
    }
    Segment(Previous, End);
    if (End > Start)
        Draw->AddLine(ImVec2(TimeTransform.TimeToX(Start), Y), ImVec2(TimeTransform.TimeToX(End), Y),
            IM_COL32(30, 30, 30, 160), 2.0f);
    constexpr float KeyRadius = 6.5f;
    constexpr float HitRadius = 10.0f;
    const ImVec2 Mouse = ImGui::GetIO().MousePos;
    const bool Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    for (const auto& Key : Keys)
    {
        if (Key.Time < 0 || Key.Time > PreviewSequence->Duration) continue;
        const float X = TimeTransform.TimeToX(Key.Time);
        if (X < PreviewData.TimelineLeft - HitRadius || X > PreviewData.TimelineRight + HitRadius) continue;
        const bool OnKey = Hovered && Mouse.x >= PreviewData.TimelineLeft && Mouse.x <= PreviewData.TimelineRight &&
            Mouse.y >= Row.ClipTop && Mouse.y <= Row.ClipBottom && std::abs(Mouse.x - X) <= HitRadius && std::abs(Mouse.y - Y) <= HitRadius;
        if (OnKey)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (Item.Type == 0) ImGui::SetTooltip("%.3fs : %.3f", Key.Time, Key.Value);
            else ImGui::SetTooltip("%.3fs : %.3f", Key.Time, Key.Value);
            if (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))
            {
                SelectedTrack = RowIndex; SelectedKeyTime = Key.Time;
                if (Item.Type == 1 && Item.Channel >= 0 && CurveChannel != Item.Channel)
                { CurveChannel = Item.Channel; CurveTrackId = -1; }
            }
            if (ImGui::IsMouseClicked(0) && !bDraggingTrackBoundary)
            {
                bDraggingKey = true;
                KeyDragStartX = Mouse.x;
                KeyDragStartTime = Key.Time;
            }
        }
        const bool Selected = SelectedTrack == RowIndex && SelectedKeyTime && *SelectedKeyTime == Key.Time;
        if (Selected) Draw->AddCircleFilled(ImVec2(X, Y), KeyRadius + 2.0f, IM_COL32(145, 145, 145, 255), 24);
        else Draw->AddCircleFilled(ImVec2(X, Y), KeyRadius + 1.0f, IM_COL32(50, 50, 50, 255), 24);
        Draw->AddCircleFilled(ImVec2(X, Y), KeyRadius, IM_COL32(245, 92, 96, 255), 24);
    }
    Draw->PopClipRect();
}

void FSequencerPanel::HandleTimelineInput(const ImVec2& Origin)
{
    const float Duration = PreviewSequence->Duration;
    const float FPS = Player.GetFramesPerSecond();
    const float Left = PreviewData.TimelineLeft;
    const float Right = PreviewData.TimelineRight;
    const float HeaderBottom = Origin.y + 20.0f;
    const auto TimeX = [&](float Time) { return TimeTransform.TimeToX(Time); };
    const float MarkerTop = Origin.y - TimelineMarkerRowHeight;
    const auto MouseTime = [&]() {
        const float Time = TimeTransform.XToTime(ImGui::GetIO().MousePos.x);
        return std::clamp(std::round(Time * FPS) / FPS, 0.0f, Duration);
    };
    const ImVec2 Mouse = ImGui::GetIO().MousePos;
    const ImRect Header(ImVec2(Left, MarkerTop), ImVec2(Right, HeaderBottom));
    // ImSequencer's ruler button owns the active ID on click; allow our overlay to handle it.
    const ImGuiHoveredFlags HoverFlags = ImGuiHoveredFlags_ChildWindows |
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;
    if (ImGui::IsWindowHovered(HoverFlags) && Header.Contains(Mouse) && ImGui::IsMouseClicked(0))
    {
        const float StartX = TimeX(Player.GetPlaybackStart());
        const float EndX = TimeX(Player.GetPlaybackEnd());
        const bool AtTop = Mouse.y < Origin.y;
        if (AtTop && std::abs(Mouse.x - StartX) <= 8.0f) DraggedTimelineHandle = 2;
        else if (AtTop && std::abs(Mouse.x - EndX) <= 8.0f) DraggedTimelineHandle = 3;
        else DraggedTimelineHandle = 1;
    }
    if (ImGui::IsWindowHovered(HoverFlags) && Header.Contains(Mouse))
    {
        const bool NearBoundary = Mouse.y < Origin.y &&
            (std::abs(Mouse.x - TimeX(Player.GetPlaybackStart())) <= 8.0f ||
             std::abs(Mouse.x - TimeX(Player.GetPlaybackEnd())) <= 8.0f);
        if (NearBoundary || std::abs(Mouse.x - TimeX(Player.GetCurrentTime())) <= 8.0f)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (!ImGui::IsMouseDown(0)) DraggedTimelineHandle = 0;
    if (DraggedTimelineHandle != 0)
    {
        const float Time = MouseTime();
        const float Gap = std::min(1.0f / FPS, Duration);
        if (DraggedTimelineHandle == 2)
            Player.SetPlaybackRange(std::clamp(Time, 0.0f, std::max(0.0f, Player.GetPlaybackEnd() - Gap)), Player.GetPlaybackEnd());
        else if (DraggedTimelineHandle == 3)
            Player.SetPlaybackRange(Player.GetPlaybackStart(), std::clamp(Time, std::min(Duration, Player.GetPlaybackStart() + Gap), Duration));
        else Player.Seek(Time);
    }

}

void FSequencerPanel::DrawTimeRuler(const ImVec2& Origin)
{
    const float Duration = PreviewSequence->Duration;
    const float FPS = Player.GetFramesPerSecond();
    const float Left = PreviewData.TimelineLeft;
    const float Right = PreviewData.TimelineRight;
    const float HeaderBottom = Origin.y + 20.0f;
    const auto TimeX = [&](float Time) { return TimeTransform.TimeToX(Time); };
    ImDrawList* Draw = ImGui::GetWindowDrawList();
    const float RawStep = 100.0f / (PreviewData.PixelsPerFrame * FPS);
    const float Magnitude = std::pow(10.0f, std::floor(std::log10(RawStep)));
    const float N = RawStep / Magnitude;
    const float Step = (N <= 1 ? 1 : N <= 2 ? 2 : N <= 5 ? 5 : 10) * Magnitude;
    const float FirstTime = TimeTransform.XToTime(Left);
    const float LastTime = std::min(Duration, TimeTransform.XToTime(Right));
    const float MinorStep = Step / 5.0f;
    const int FirstTick = int(std::floor(FirstTime / MinorStep));
    const int LastTick = int(std::ceil(LastTime / MinorStep));
    const int Precision = std::clamp(int(-std::floor(std::log10(Step))), 0, 4);
    for (int I = FirstTick; I <= LastTick; ++I)
    {
        const float X = TimeX(I * MinorStep);
        const bool Major = I % 5 == 0;
        Draw->AddLine(ImVec2(X, Origin.y + (Major ? 7 : 14)), ImVec2(X, HeaderBottom),
            Major ? IM_COL32(180, 180, 180, 255) : IM_COL32(100, 100, 100, 255));
        if (!Major) continue;
        char Label[32];
        ImFormatString(Label, sizeof(Label), "%.*f", Precision, I * MinorStep);
        Draw->AddText(ImVec2(X + 3, Origin.y), IM_COL32(180, 180, 180, 255), Label);
    }
}

void FSequencerPanel::DrawTimelineOverlay(const ImVec2& Origin, const ImVec2& Size)
{
    const float Left = PreviewData.TimelineLeft;
    const float Right = PreviewData.TimelineRight;
    const float MarkerTop = Origin.y - TimelineMarkerRowHeight;
    const float HeaderBottom = Origin.y + 20.0f; // Original ImSequencer ruler height.
    const float Bottom = Origin.y + Size.y - 22.0f; // Original scrollbar reservation.
    if (Right <= Left || Bottom <= HeaderBottom) return;
    const auto TimeX = [&](float Time) { return TimeTransform.TimeToX(Time); };
    HandleTimelineInput(Origin);

    ImDrawList* Draw = ImGui::GetWindowDrawList();
    Draw->PushClipRect(ImVec2(Left, MarkerTop), ImVec2(Right, Bottom), true);
    Draw->AddRectFilled(ImVec2(Left, MarkerTop), ImVec2(Right, Origin.y), IM_COL32(24, 24, 24, 255));
    Draw->AddRectFilled(ImVec2(Left, Origin.y), ImVec2(Right, HeaderBottom), IM_COL32(30, 30, 30, 255));
    DrawTimeRuler(Origin);
    const auto Boundary = [&](float Time, bool Start) {
        const float X = TimeX(Time);
        const float D = Start ? 1.0f : -1.0f;
        const ImU32 Color = Start ? IM_COL32(75, 190, 90, 255) : IM_COL32(220, 45, 50, 255);
        Draw->AddLine(ImVec2(X, MarkerTop), ImVec2(X, Bottom), Color, 1.5f);
        Draw->AddTriangleFilled(ImVec2(X, MarkerTop), ImVec2(X + D * 10, MarkerTop), ImVec2(X, MarkerTop + 10), Color);
        Draw->AddTriangleFilled(ImVec2(X, Bottom), ImVec2(X + D * 8, Bottom), ImVec2(X, Bottom - 8), Color);
    };
    Boundary(Player.GetPlaybackStart(), true);
    Boundary(Player.GetPlaybackEnd(), false);
    const float X = TimeX(Player.GetCurrentTime());
    if (X >= Left && X <= Right)
    {
        const ImU32 Red = IM_COL32(255, 60, 65, 255);
        Draw->AddLine(ImVec2(X, Origin.y), ImVec2(X, Bottom), IM_COL32(235, 235, 235, 255), 1.5f);
        Draw->AddRectFilled(ImVec2(X - 8, MarkerTop), ImVec2(X + 8, Origin.y - 6), Red);
        Draw->AddTriangleFilled(ImVec2(X - 8, Origin.y - 6), ImVec2(X + 8, Origin.y - 6), ImVec2(X, Origin.y), Red);
        char Label[32];
        ImFormatString(Label, sizeof(Label), "%.4f", Player.GetCurrentTime());
        const float Width = ImGui::CalcTextSize(Label).x;
        const float LabelX = X + 12 + Width <= Right ? X + 12 : X - 12 - Width;
        Draw->AddRectFilled(ImVec2(LabelX - 2, MarkerTop), ImVec2(LabelX + Width + 2, Origin.y), IM_COL32(24, 24, 24, 255));
        Draw->AddText(ImVec2(LabelX, MarkerTop + 3), IM_COL32(255, 255, 255, 255), Label);
    }
    Draw->PopClipRect();
}
