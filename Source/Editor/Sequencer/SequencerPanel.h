#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Sequencer/SequencePlayer.h"
#include "ThirdParty/ImSequencer/ImSequencer.h"
#include <vector>
#include <string>
#include <filesystem>

class APlayerCameraManager;

struct FSequencerTimeTransform
{
    float VisibleStartTime = 0.0f;
    float PixelsPerSecond = 1.0f;
    float TimelineX = 0.0f;
    float TimeToX(float Time) const { return TimelineX + (Time - VisibleStartTime) * PixelsPerSecond; }
    float XToTime(float X) const { return VisibleStartTime + (X - TimelineX) / PixelsPerSecond; }
};

// Adapter data for the editor preview, not a runtime Sequence Asset.
struct FSequencerPreviewData : ImSequencer::SequenceInterface
{
    struct FItem
    {
        int Type = 0;
        int Start = 0;
        int End = 0;
        std::string Name;
        int Id = 0;
        int TrackIndex = -1; // Editor metadata index; points to the source track.
        int SectionIndex = -1;
        int TrackId = 0;
    };
    std::vector<FItem> Items;
    int FirstFrame = -15;
    int LastFrame = 150;
    bool bHasGeometry = false;
    float PixelsPerFrame = 1.0f;
    float ZeroFrameX = 0.0f;
    float TimelineLeft = 0.0f;
    float TimelineRight = 0.0f;
    struct FRowGeometry { float Top = 0; float Bottom = 0; float ClipTop = 0; float ClipBottom = 0; };
    std::vector<FRowGeometry> Rows;
    float PendingScroll = 0.0f;
    size_t GetCustomHeight(int) override { return 18; }
    void CustomDraw(int Index, ImDrawList* DrawList, const ImRect& Rect, const ImRect&, const ImRect& Clip, const ImRect&) override;
    void CustomDrawCompact(int Index, ImDrawList* DrawList, const ImRect& Rect, const ImRect& Clip) override;
    int GetFrameMin() const override { return FirstFrame; }
    int GetFrameMax() const override { return LastFrame; }
    int GetItemCount() const override { return static_cast<int>(Items.size()); }
    int GetItemTypeCount() const override { return 2; }
    const char* GetItemTypeName(int Type) const override { return Type == 0 ? "Fade" : "Camera"; }
    // Labels are drawn by the panel; the library still owns row layout and scrolling.
    const char* GetItemLabel(int /*Index*/) const override { return ""; }
    void Get(int Index, int** Start, int** End, int* Type, unsigned int* Color) override;
};

// The panel owns UI state and forwards playback operations to the runtime player.
class FSequencerPanel : public IEditorPanel
{
public:
    bool Init() override;
    void SetCameraManagerProvider(std::function<APlayerCameraManager*()> Provider);
    void Tick(float DeltaTime) override;
    void OnRender() override;
    const char* GetPanelName() const override { return "Sequencer"; }

private:
    void SetInitialDock();
    void DrawToolbar();
    void NewSequence();
    void OpenSequenceDialog(bool bSave);
    bool SaveSequenceJson(const std::filesystem::path& Path);
    bool LoadSequenceJson(const std::filesystem::path& Path);
    std::filesystem::path SequencePath;
    std::string FileError;
    void DrawTimeline();
    void DrawTransportBar();
    void SyncSequence();
    void RebuildItemsFromSequence(int TrackId = -1, int SectionId = -1);
    void DrawContextMenu();
    int ContextTrackId = 0;
    bool bTimelineContext = false;
    float ContextTime = 0.0f;
    FSequenceSection* GetSelectedSection();
    void AddKey(float Time);
    void DeleteSelectedKey();
    void EditSelectedKey(float Time, float Value);
    void DrawKeys();
    void DrawTrackKeyControls(int RowIndex, float Left, float Right);
    void SetCurrentFadeValue(float Value);
    std::optional<float> FindAdjacentKey(int Direction);
    void JumpToKey(int Direction);
    std::optional<float> SelectedKeyTime;
    bool bDraggingKey = false;
    float KeyDragStartX = 0.0f;
    float KeyDragStartTime = 0.0f;
    void AddTrack(int Type);
    void DeleteTrack(int TrackIndex);
    std::vector<FSequenceSection>& GetSections(int TrackIndex);
    int GetSelectedTrackIndex() const;
    void NotifySequenceEdited();
    void DrawTimelineOverlay(const ImVec2& Origin, const ImVec2& Size);
    void DrawTrackList(const ImVec2& Origin, const ImVec2& Size);
    FSequencerTimeTransform TimeTransform;
    float TrackListWidth = 300.0f;
    bool bDraggingTrackBoundary = false;
    int DraggedTimelineHandle = 0; // 0: none, 1: playhead, 2: start, 3: end

    FSequencePlayer Player;
    std::shared_ptr<FSequence> PreviewSequence = std::make_shared<FSequence>();
    // Names/IDs are editor metadata. Camera data stays here until camera playback exists.
    struct FEditorTrack
    {
        int Id = 0;
        int Type = 0;
        std::string Name;
        int RuntimeTrackIndex = -1;
        std::vector<int> SectionIds;
        std::vector<FSequenceSection> PlaceholderSections;
    };
    std::vector<FEditorTrack> EditorTracks;
    int NextEditorId = 1;
    float CachedFrameRate = 0.0f;
    std::function<APlayerCameraManager*()> CameraManagerProvider;
    bool bPreviewRequested = false;
    bool bInitialDockPending = true;
    int SelectedTrack = -1;
    FSequencerPreviewData PreviewData;
    int FirstVisibleFrame = 0;
    bool bExpanded = true;
};
