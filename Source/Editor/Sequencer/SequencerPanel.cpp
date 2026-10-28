#include "EnginePCH.h"
#include "SequencerPanel.h"

#include <algorithm>
#include <cmath>

namespace
{
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

void FSequencerPanel::Tick(float DeltaTime)
{
    if (IsOpen() && bPreviewPlaying)
    {
        CurrentTime += std::max(DeltaTime, 0.0f) * PlaybackDirection;
        if (CurrentTime >= PlaybackEnd || CurrentTime <= PlaybackStart)
        {
            const float Span = PlaybackEnd - PlaybackStart;
            if (bLoop && Span > 0.0f)
            {
                float Offset = std::fmod(CurrentTime - PlaybackStart, Span);
                if (Offset < 0) Offset += Span;
                CurrentTime = PlaybackStart + Offset;
            }
            else
            {
                CurrentTime = std::clamp(CurrentTime, PlaybackStart, PlaybackEnd);
                bPreviewPlaying = false;
            }
        }
    }
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
        ImGui::Separator();
        DrawTimeline();
    }
    ImGui::End();
}

void FSequencerPanel::DrawToolbar()
{
    const auto NextButton = [](const char* Label)
    {
        NextToolbarControl(ImGui::CalcTextSize(Label).x + ImGui::GetStyle().FramePadding.x * 2);
    };

    // Asset and key editing commands are placeholders until their systems are connected.
    ImGui::BeginDisabled();
    ImGui::Button("New");
    NextButton("Open"); ImGui::Button("Open");
    NextButton("Save"); ImGui::Button("Save");
    ImGui::EndDisabled();

    NextButton("+ Add");
    if (ImGui::Button("+ Add")) ImGui::OpenPopup("AddTrack");
    if (ImGui::BeginPopup("AddTrack"))
    {
        for (int Type = 0; Type < PreviewData.GetItemTypeCount(); ++Type)
        {
            if (ImGui::MenuItem(PreviewData.GetItemTypeName(Type)))
            {
                PreviewData.LastFrame = std::max(1, static_cast<int>(std::lround(Duration * FramesPerSecond)));
                PreviewData.Add(Type);
                SelectedTrack = PreviewData.GetItemCount() - 1;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::BeginDisabled();
    NextButton("+ Key"); ImGui::Button("+ Key");
    NextButton("Delete Key"); ImGui::Button("Delete Key");
    NextButton("Fit"); ImGui::Button("Fit");
    ImGui::EndDisabled();

    DrawTransportBar();
    NextToolbarControl(100.0f + ImGui::CalcTextSize("Time (s)").x + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::DragFloat("Time (s)", &CurrentTime, 0.01f, 0.0f, Duration, "%.2f"))
    {
        if (!std::isfinite(CurrentTime)) CurrentTime = 0.0f;
        CurrentTime = std::clamp(CurrentTime, 0.0f, Duration);
        bPreviewPlaying = false;
    }
    NextToolbarControl(100.0f + ImGui::CalcTextSize("Length (s)").x + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(100.0f);
    const float PreviousDuration = Duration;
    if (ImGui::DragFloat("Length (s)", &Duration, 0.1f, 0.1f, 3600.0f, "%.2f"))
    {
        if (!std::isfinite(Duration)) Duration = 5.0f;
        Duration = std::clamp(Duration, 0.1f, 3600.0f);
        CurrentTime = std::min(CurrentTime, Duration);
        PlaybackEnd = PlaybackEnd == PreviousDuration ? Duration : std::min(PlaybackEnd, Duration);
        PlaybackStart = std::clamp(PlaybackStart, 0.0f, std::max(0.0f, PlaybackEnd - 1.0f / FramesPerSecond));
    }
}

void FSequencerPanel::TogglePlayback(int Direction)
{
    if (bPreviewPlaying && PlaybackDirection == Direction)
    {
        bPreviewPlaying = false;
        return;
    }
    PlaybackDirection = Direction;
    if (Direction > 0 && (CurrentTime >= PlaybackEnd || CurrentTime < PlaybackStart)) CurrentTime = PlaybackStart;
    if (Direction < 0 && (CurrentTime <= PlaybackStart || CurrentTime > PlaybackEnd)) CurrentTime = PlaybackEnd;
    bPreviewPlaying = true;
}

void FSequencerPanel::DrawTransportBar()
{
    const auto Button = [](const char* Id, const char* Tooltip, ETransportIcon Icon, bool Active = false)
    {
        NextToolbarControl(ImGui::GetFrameHeight());
        return TransportButton(Id, Tooltip, Icon, Active);
    };
    ImGui::BeginDisabled();
    TransportButton("##Record", "Record", ETransportIcon::Record);
    ImGui::EndDisabled();
    if (Button("##RangeStart", "Set playback start", ETransportIcon::RangeStart))
        PlaybackStart = std::clamp(CurrentTime, 0.0f, std::max(0.0f, PlaybackEnd - 1.0f / FramesPerSecond));
    if (Button("##First", "First", ETransportIcon::First))
    {
        CurrentTime = PlaybackStart;
        bPreviewPlaying = false;
    }
    ImGui::BeginDisabled();
    Button("##PreviousKey", "Previous key", ETransportIcon::PreviousKey);
    ImGui::EndDisabled();
    if (Button("##PreviousFrame", "Previous frame", ETransportIcon::PreviousFrame))
    {
        CurrentTime = std::clamp((std::round(CurrentTime * FramesPerSecond) - 1) / FramesPerSecond, 0.0f, Duration);
        bPreviewPlaying = false;
    }
    const bool ReversePlaying = bPreviewPlaying && PlaybackDirection < 0;
    if (Button("##Reverse", ReversePlaying ? "Pause" : "Play reverse",
        ReversePlaying ? ETransportIcon::Pause : ETransportIcon::Reverse, ReversePlaying)) TogglePlayback(-1);
    const bool ForwardPlaying = bPreviewPlaying && PlaybackDirection > 0;
    if (Button("##Play", ForwardPlaying ? "Pause" : "Play",
        ForwardPlaying ? ETransportIcon::Pause : ETransportIcon::Play, ForwardPlaying)) TogglePlayback(1);
    if (Button("##NextFrame", "Next frame", ETransportIcon::NextFrame))
    {
        CurrentTime = std::clamp((std::round(CurrentTime * FramesPerSecond) + 1) / FramesPerSecond, 0.0f, Duration);
        bPreviewPlaying = false;
    }
    ImGui::BeginDisabled();
    Button("##NextKey", "Next key", ETransportIcon::NextKey);
    ImGui::EndDisabled();
    if (Button("##Last", "Last", ETransportIcon::Last))
    {
        CurrentTime = PlaybackEnd;
        bPreviewPlaying = false;
    }
    if (Button("##RangeEnd", "Set playback end", ETransportIcon::RangeEnd))
        PlaybackEnd = std::clamp(CurrentTime, PlaybackStart + 1.0f / FramesPerSecond, Duration);
    if (Button("##Loop", bLoop ? "Loop playback" : "Play once",
        bLoop ? ETransportIcon::Loop : ETransportIcon::Once, bLoop)) bLoop = !bLoop;
}

void FSequencerPreviewData::Get(int Index, int** Start, int** End, int* Type, unsigned int* Color)
{
    auto& Item = Items[Index];
    if (Start) *Start = &Item.Start;
    if (End) *End = &Item.End;
    if (Type) *Type = Item.Type;
    if (Color) *Color = Item.Type == 0 ? IM_COL32(95, 165, 220, 255) : IM_COL32(170, 125, 210, 255);
}

void FSequencerPreviewData::Add(int Type)
{
    Items.push_back({ Type, 0, LastFrame });
}

void FSequencerPreviewData::Del(int Index)
{
    if (Index >= 0 && Index < GetItemCount()) Items.erase(Items.begin() + Index);
}

void FSequencerPanel::DrawTimeline()
{
    PreviewData.LastFrame = std::max(1, static_cast<int>(std::lround(Duration * FramesPerSecond)));
    PreviewData.PlaybackStartFrame = static_cast<int>(std::lround(PlaybackStart * FramesPerSecond));
    PreviewData.PlaybackEndFrame = static_cast<int>(std::lround(PlaybackEnd * FramesPerSecond));
    PreviewData.DisplayFrame = CurrentTime * FramesPerSecond;
    FirstVisibleFrame = std::clamp(FirstVisibleFrame, 0, PreviewData.LastFrame - 1);
    for (auto& Item : PreviewData.Items)
    {
        Item.Start = std::clamp(Item.Start, 0, PreviewData.LastFrame);
        Item.End = std::clamp(Item.End, Item.Start, PreviewData.LastFrame);
    }
    const float Height = std::max(1.0f, ImGui::GetContentRegionAvail().y);
    if (ImGui::BeginChild("Timeline", ImVec2(0, Height), ImGuiChildFlags_Borders))
    {
        // The widget needs room for its legend, ruler, rows and scrollbar.
        if (ImGui::GetContentRegionAvail().x >= 240.0f &&
            ImGui::GetContentRegionAvail().y >= 80.0f && !PreviewData.Items.empty())
        {
            int Frame = static_cast<int>(std::lround(CurrentTime * FramesPerSecond));
            const int PreviousFrame = Frame;
            ImSequencer::Sequencer(&PreviewData, &Frame, &bExpanded, &SelectedTrack,
                &FirstVisibleFrame, ImSequencer::SEQUENCER_EDIT_STARTEND |
                ImSequencer::SEQUENCER_CHANGE_FRAME | ImSequencer::SEQUENCER_DEL);
            if (Frame != PreviousFrame)
            {
                CurrentTime = std::clamp(Frame / float(FramesPerSecond), 0.0f, Duration);
                bPreviewPlaying = false;
            }
        }
    }
    ImGui::EndChild();
}
