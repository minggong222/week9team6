#pragma once

#include "Editor/EditorUI/EditorPanel.h"

#include <filesystem>

namespace fs = std::filesystem;

class UWorld;

class FContentDrawerPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Content Drawer"; }
	void RenderFolderTree(const fs::path& Path, fs::path& SelectedPath);
    void SelectAsset(const FString& Path);
    void SetWorld(UWorld* InWorld) { World = InWorld; PendingRenamePath.clear(); }


private:
    void DrawRenameDialog();
    UWorld* World = nullptr;
    fs::path SelectedAssetPath;
    fs::path PendingRenamePath;
    char RenameBuffer[256] = {};
    FString RenameError;
    bool bOpenRenameDialog = false;
    bool bRevealSelection = false;

	fs::path AssetRootPath = "Assets";
	fs::path CurrentPath = "Assets";

	const FString FolderIconPath = "Assets/Icons/Folder.png";
	const FString FileIconPath = "Assets/Icons/File.png";
	
};