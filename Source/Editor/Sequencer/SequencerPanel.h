#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Sequencer/SequencePlayer.h"
#include "ThirdParty/ImSequencer/ImSequencer.h"
#include <vector>
#include <string>
#include <filesystem>
#include <array>
#include "Camera/CameraViewInfo.h"

class APlayerCameraManager;
class UWorld;
class UCameraComponent;
struct FViewportCameraInput;

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
        int Channel = -1; // Camera: XYZ, Pitch/Yaw/Roll, FOV, Projection, OrthoWidth.
        int Group = 0; // 1: Location, 2: Rotation, 3: Camera.
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
    void SetWorldProvider(std::function<UWorld*()> Provider) { WorldProvider = std::move(Provider); }
    void SetCameraPreviewCallback(std::function<void(std::optional<FCameraPOV>)> Callback) { CameraPreviewCallback = std::move(Callback); }
    void SetFadePreviewCallback(std::function<void(std::optional<float>)> Callback) { FadePreviewCallback = std::move(Callback); }
    bool IsCameraPilotEnabled() const;
    void OnCameraViewEdited(const FCameraPOV& POV);
    void TickPIECameraPilot(const FViewportCameraInput& Input, float DeltaTime,
        float MoveSpeed, float MouseSensitivity, float ViewWidth);
    void ReleaseEditorCameraPreview();
    void ReleasePreview();
    void Tick(float DeltaTime) override;
    void OnRender() override;
    const char* GetPanelName() const override { return "Sequencer"; }

private:
    // Sequence lifecycle and file operations.
    void NewSequence();
    void ResetEditorState(float FPS);
    void OpenSequenceDialog(bool bSave);
    void DrawSequenceFileDialog();
    bool SaveSequenceJson(const std::filesystem::path& Path);
    bool LoadSequenceJson(const std::filesystem::path& Path);

    // Toolbar and playback controls.
    void SetInitialDock();
    void DrawToolbar();
    void DrawFileControls();
    void DrawAddTrackButton();
    void DrawTransportBar();
    void DrawPlaybackSettings();
    void DrawCurveEditor();
    bool bCurveEditorOpen = false;
    int CurveChannel = 0;
    int CurveTrackId = -1;
    int CurvePointCount = -1;
    int CurveEditorSerial = 0;
    ImVec2 CurveMin{};
    ImVec2 CurveMax{};

    // Camera binding, key editing and evaluated view application.
    void DrawCameraSettings();
    std::vector<UCameraComponent*> GetCameraChoices() const;
    UCameraComponent* ResolveCamera(const std::string& Actor, const std::string& Component) const;
    void ApplyCameraResult(std::optional<FSequenceEvaluatedValue> Result);
    FSequenceCameraValue GetCameraKeyValue(float Time) const;
    void EditSelectedCameraKey(float Time, const FSequenceCameraValue& Value);

    // Sequence editing and the ImSequencer row adapter.
    void SyncSequence();
    void RebuildItemsFromSequence(int TrackId = -1, int SectionId = -1);

    void AddTrack(int Type);
    void DeleteTrack(int TrackIndex);
    std::vector<FSequenceSection>& GetSections(int TrackIndex);
    int GetSelectedTrackIndex() const;
    void NotifySequenceEdited();
    FSequenceSection* GetSelectedSection();
    std::vector<int> GetRowChannels(int RowIndex) const;
    std::vector<FSequenceKeyframe> GetRowKeys(int RowIndex) const;
    void SelectCameraChannel(int Channel);

    void AddKey(float Time);
    void DeleteSelectedKey();
    void DrawSelectedKeyInterpolation();
    void SetSelectedKeyInterpolation(std::optional<ESequenceInterpolation> Mode);
    void EditSelectedKey(float Time, float Value);
    void SetCurrentFadeValue(float Value);
    std::optional<float> FindAdjacentKey(int Direction);
    void JumpToKey(int Direction);

    // Timeline drawing and interaction.
    void DrawTimeline();
    void DrawContextMenu();
    void DrawKeys();
    void DrawTrackKeys(int RowIndex);
    void DrawTrackKeyControls(int RowIndex, float Left, float Right);
    void DrawTimelineOverlay(const ImVec2& Origin, const ImVec2& Size);
    void HandleTimelineInput(const ImVec2& Origin);
    void DrawTimeRuler(const ImVec2& Origin);
    void DrawTrackList(const ImVec2& Origin, const ImVec2& Size);

    // Runtime data and playback. Playback state belongs to Player.
    FSequencePlayer Player;
    std::shared_ptr<FSequence> PreviewSequence = std::make_shared<FSequence>();
    std::function<APlayerCameraManager*()> CameraManagerProvider;
    bool bPreviewRequested = false;
    bool bCameraPreviewRequested = false;
    bool bCameraPreviewEnabled = false;
    bool bCameraPilotEnabled = false;
    std::optional<FSequenceCameraValue> PilotCameraValue;
    float PilotCameraTime = 0.0f;
    std::function<UWorld*()> WorldProvider;
    std::function<void(std::optional<FCameraPOV>)> CameraPreviewCallback;
    std::function<void(std::optional<float>)> FadePreviewCallback;

    // Editor-only metadata and file state.
    // Names/IDs are editor metadata; all track values live in the runtime Sequence.
    struct FEditorTrack
    {
        int Id = 0;
        int Type = 0;
        std::string Name;
        int RuntimeTrackIndex = -1;
        std::vector<int> SectionIds;
    };
    std::vector<FEditorTrack> EditorTracks;
    int NextEditorId = 1;
    float CachedFrameRate = 0.0f;
    std::filesystem::path SequencePath;
    std::string FileError;
    std::array<char, 1024> SequenceFileName{};
    std::vector<std::filesystem::path> SequenceFileChoices;
    bool bSaveSequenceDialog = false;
    bool bConfirmSequenceOverwrite = false;

    // Selection and context menu.
    int SelectedTrack = -1;
    std::optional<float> SelectedKeyTime;
    int ContextTrackId = 0;
    bool bTimelineContext = false;
    float ContextTime = 0.0f;

    // Timeline layout and drag state.
    FSequencerTimeTransform TimeTransform;
    float TrackListWidth = 410.0f;
    bool bCameraExpanded = true;
    bool bCameraLocationExpanded = true;
    bool bCameraRotationExpanded = true;
    bool bDraggingTrackBoundary = false;
    int DraggedTimelineHandle = 0; // 0: none, 1: playhead, 2: start, 3: end
    bool bDraggingKey = false;
    float KeyDragStartX = 0.0f;
    float KeyDragStartTime = 0.0f;
    bool bInitialDockPending = true;
    FSequencerPreviewData PreviewData;
    int FirstVisibleFrame = 0;
    bool bExpanded = true;
};
