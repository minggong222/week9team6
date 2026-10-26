#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/EditorUI/ActorNameEdit.h"
#include "Core/EngineString.h"
#include "Core/Types.h"

#include <functional>
#include <vector>

class UWorld;
class ULevel;
class AActor;
class UActorComponent;
class UPrimitiveComponent;
class USceneComponent;

using SelectionCallback = std::function<void(USceneComponent*)>;
using DeleteActorCallback = std::function<void(AActor*)>;

class FOutlinerPanel : public IEditorPanel
{
public:
    FOutlinerPanel() = default;
    ~FOutlinerPanel() = default;

    void SetSelectionCallback(SelectionCallback InCallback) { Callback = InCallback; }
    void SetDeleteActorCallback(DeleteActorCallback InCallback) { DeleteCallback = InCallback;}

public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
    const char* GetPanelName() const override { return "Outliner"; }

    void SetWorld(UWorld* InWorld)
    {
        World = InWorld;
        CachedLevel = nullptr;
        SortedActors.clear();
        FilteredActorIndices.clear();
        SelectedObject = nullptr;
        RenameActor = nullptr;
        bOpenRenamePopup = false;
        PendingDeleteActor = nullptr;
        bActorListDirty = true;
        bFilterDirty = true;
    }

    void DrawActors(ULevel* Level);
    void DrawActorNode(AActor* Actor);
    void SelectActor(AActor* Actor);

    AActor* GetSelectedActor() const;


private:
    struct FActorEntry
    {
        AActor* Actor;
        FString Name;
    };

    std::vector<FActorEntry> SortedActors;
    std::vector<size_t> FilteredActorIndices;
    ULevel* CachedLevel = nullptr;
    uint64 CachedActorListRevision = 0;
    bool bActorListDirty = true;
    bool bFilterDirty = true;
    char SearchText[128] = {};

    void BeginRename(AActor* Actor);
    void DrawRenamePopup(ULevel* Level);
    AActor* RenameActor = nullptr;
    uint32 RenameActorUUID = 0;
    bool bOpenRenamePopup = false;
    FActorNameEdit RenameEdit;

    UWorld* World = nullptr;
    UObject* SelectedObject = nullptr;

    SelectionCallback Callback;
    DeleteActorCallback DeleteCallback;
    AActor* PendingDeleteActor = nullptr;
};
