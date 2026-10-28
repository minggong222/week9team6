#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "ThirdParty/ImSequencer/ImSequencer.h"
#include <vector>

// Adapter data for the editor preview, not a runtime Sequence Asset.
struct FSequencerPreviewData : ImSequencer::SequenceInterface
{
    struct FItem { int Type; int Start; int End; };
    std::vector<FItem> Items = { { 0, 0, 30 }, { 1, 0, 150 } };
    int LastFrame = 150;
    int PlaybackStartFrame = 0;
    int PlaybackEndFrame = 150;
    float DisplayFrame = 0.0f;
    float GetFramesPerSecond() const override { return 30.0f; }
    int GetPlaybackStart() const override { return PlaybackStartFrame; }
    int GetPlaybackEnd() const override { return PlaybackEndFrame; }
    float GetCurrentFrame() const override { return DisplayFrame; }
    int GetFrameMin() const override { return 0; }
    int GetFrameMax() const override { return LastFrame; }
    int GetItemCount() const override { return static_cast<int>(Items.size()); }
    int GetItemTypeCount() const override { return 2; }
    const char* GetItemTypeName(int Type) const override { return Type == 0 ? "Fade" : "Camera"; }
    const char* GetItemLabel(int Index) const override { return GetItemTypeName(Items[Index].Type); }
    void Get(int Index, int** Start, int** End, int* Type, unsigned int* Color) override;
    void Add(int Type) override;
    void Del(int Index) override;
    void Duplicate(int Index) override
    {
        if (Index >= 0 && Index < GetItemCount())
        {
            const FItem Item = Items[Index];
            Items.push_back(Item);
        }
    }
};

// UI state only. Sequence Asset and Sequence Player will be connected later.
class FSequencerPanel : public IEditorPanel
{
public:
    bool Init() override { return true; }
    void Tick(float DeltaTime) override;
    void OnRender() override;
    const char* GetPanelName() const override { return "Sequencer"; }

private:
    void SetInitialDock();
    void DrawToolbar();
    void DrawTimeline();
    void DrawTransportBar();
    void TogglePlayback(int Direction);

    float CurrentTime = 0.0f;
    float Duration = 5.0f;
    bool bPreviewPlaying = false;
    int PlaybackDirection = 1;
    bool bLoop = false;
    float PlaybackStart = 0.0f;
    float PlaybackEnd = 5.0f;
    bool bInitialDockPending = true;
    int SelectedTrack = -1;
    FSequencerPreviewData PreviewData;
    int FirstVisibleFrame = 0;
    bool bExpanded = true;
    static constexpr int FramesPerSecond = 30;
};
