#include "EnginePCH.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"
#include "Asset/AssetManager.h"
#include "Text/Font.h"
#include "Editor/HitoriEd/EditorDragDrop.h"
#include "Editor/HitoriEd/ScriptEditorUtils.h"
#include "Engine/World.h"

namespace
{
    FString Utf8AssetPath(const fs::path& Path)
    {
        const auto Text = Path.generic_u8string();
        return FString(Text.begin(), Text.end());
    }
}

void FContentDrawerPanel::SelectAsset(const FString& Path)
{
    SelectedAssetPath = fs::u8path(Path).lexically_normal();
    CurrentPath = SelectedAssetPath.parent_path();
    bRevealSelection = true;
}


bool FContentDrawerPanel::Init()
{
	return false;
}

void FContentDrawerPanel::Tick(float DeltaTime)
{
}

void FContentDrawerPanel::OnRender()
{
	ImGui::Begin("Content Drawer");
	std::error_code Error;

	if (!fs::is_directory(CurrentPath, Error))
	{
		CurrentPath = AssetRootPath;
	}

	Error.clear();

	if (!fs::is_directory(AssetRootPath, Error))
	{
		ImGui::TextUnformatted("Asset folder is unavailable.");
		ImGui::End();
		return;
	}

	float treeViewWidth = 200.0f;
	ImGui::BeginChild("FolderTreeView", ImVec2(treeViewWidth, 0));
	{
		// 루트 노드
		if (ImGui::TreeNodeEx(AssetRootPath.string().c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
			(AssetRootPath == CurrentPath ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_DrawLinesToNodes))
		{
			if (ImGui::IsItemClicked())
			{
				CurrentPath = AssetRootPath;
			}
			RenderFolderTree(AssetRootPath, CurrentPath);
			ImGui::TreePop();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("AssetGridView");
	{
		// 그리드 레이아웃 계산
		float padding = 16.0f;
		float thumbnailSize = 80.0f;
		float cellSize = thumbnailSize + padding;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int ColumnCount = static_cast<int>(panelWidth / cellSize);
		if (ColumnCount < 1) ColumnCount = 1;

		// 테이블 기반 그리드 시작
		if (ImGui::BeginTable("AssetGridTable", ColumnCount))
		{
			std::error_code IterateError;
			fs::directory_iterator It(CurrentPath, IterateError);
			const fs::directory_iterator End;

			for (; !IterateError && It != End; It.increment(IterateError))
			{
				const fs::path Path = It->path();

				std::error_code TypeError;
				const bool bIsDirectory = fs::is_directory(Path, TypeError);

				if (TypeError)
					continue;

				ImGui::TableNextColumn();

				std::string PathString = Utf8AssetPath(Path);
				std::string FilenameString = Utf8AssetPath(Path.filename());

				ImGui::PushID(PathString.c_str()); // 각 위젯에 고유 ID 부여
				UTexture2D* Thumbnail = nullptr;

				// 실제 에셋일 때만 채워진다.
				// 폴더/파일 아이콘은 끌 수 없어야 하므로 썸네일과 구분해서 들고 있는다.
				UTexture2D* TextureAsset = nullptr;
				UFont* FontAsset = nullptr;

				// 폴더 또는 파일 아이콘 표시
				if (bIsDirectory)
				{
					Thumbnail = UAssetManager::GetAssetByPath<UTexture2D>(FolderIconPath);
				}
				else // 파일인 경우
				{
					URenderAsset* Asset = UAssetManager::GetAssetByPath<URenderAsset>(PathString);
					if (Asset && Asset->IsA<UTexture2D>())
					{
						TextureAsset = Cast<UTexture2D>(Asset);
						Thumbnail = TextureAsset;
					}
					else if (Asset && Asset->IsA<UFont>())
					{
						FontAsset = Cast<UFont>(Asset);

						// 폰트는 글리프 아틀라스를 그대로 미리보기로 쓴다
						Thumbnail = FontAsset->AtlasTexture;
					}

					if (Thumbnail == nullptr)
					{
						Thumbnail = UAssetManager::GetAssetByPath<UTexture2D>(FileIconPath);
					}
				}
                const bool bSelected = Path.lexically_normal() == SelectedAssetPath;
                if (bSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
                if (Thumbnail && Thumbnail->GetResource())
                    ImGui::ImageButton(PathString.c_str(), (ImTextureID)Thumbnail->GetResource()->GetSRV(), { thumbnailSize, thumbnailSize });
                else
                    ImGui::Button(FilenameString.c_str(), { thumbnailSize, thumbnailSize });
                if (bSelected) ImGui::PopStyleColor();

                const bool bItemHovered = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) SelectedAssetPath = Path.lexically_normal();
                if (bSelected && bRevealSelection)
                {
                    ImGui::SetScrollHereY();
                    bRevealSelection = false;
                }
                if (!bIsDirectory && Path.extension() == ".lua" && Path.filename() != "template.lua" &&
                    ImGui::BeginPopupContextItem("LuaAssetActions"))
                {
                    const bool bCanRename = World && World->GetWorldType() == EWorldType::Editor && !World->HasBegunPlay();
                    if (ImGui::MenuItem("Rename", nullptr, false, bCanRename))
                    {
                        SelectedAssetPath = Path.lexically_normal();
                        PendingRenamePath = Path;
                        const FString Name = Utf8AssetPath(Path.stem());
                        strncpy_s(RenameBuffer, sizeof(RenameBuffer), Name.c_str(), _TRUNCATE);
                        RenameError.clear();
                        bOpenRenameDialog = true;
                    }
                    ImGui::EndPopup();
                }

				// 페이로드에 포인터를 그대로 실어서 받는 쪽이 AssetMap을 다시 뒤지지 않게 한다.
				if (TextureAsset && ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(EditorDragDrop::Texture, &TextureAsset, sizeof(UTexture2D*));

					// 끌고 다니는 동안 커서를 따라다니는 미리보기
					ImGui::Image((ImTextureID)TextureAsset->GetResource()->GetSRV(), { thumbnailSize, thumbnailSize });
					ImGui::TextUnformatted(FilenameString.c_str());

					ImGui::EndDragDropSource();
				}
				if (FontAsset && ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(EditorDragDrop::Font, &FontAsset, sizeof(UFont*));

					ImGui::TextUnformatted(FilenameString.c_str());

					ImGui::EndDragDropSource();
				}
				// 더블 클릭 이벤트 처리
				if (bItemHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (bIsDirectory)
					{
						CurrentPath /= Path.filename(); // 하위 폴더로 이동
					}
                    else if (Path.extension() == ".lua")
                    {
                        const fs::path ScriptRoot = fs::u8path(FScriptEditorUtils::GetScriptRoot());
                        const fs::path RelativePath = Path.lexically_relative(ScriptRoot);
                        FScriptEditorUtils::EditScript(Utf8AssetPath(RelativePath));
                    }
				}
				// 파일 이름 표시
				ImGui::TextWrapped("%s", FilenameString.c_str());

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::EndChild();
	DrawRenameDialog();
	ImGui::End();
}

void FContentDrawerPanel::RenderFolderTree(const fs::path& Path, fs::path& SelectedPath)
{
	std::error_code IterateError;
	fs::directory_iterator It(Path, IterateError);
	const fs::directory_iterator End;

	for (; !IterateError && It != End; It.increment(IterateError))
	{
		const fs::path Entry = It->path();

		std::error_code TypeError;
		const bool bIsDirectory =
			fs::is_directory(Entry, TypeError);

		if (TypeError || !bIsDirectory)
			continue;

		const FString FilenameString =
			Entry.filename().string();

		ImGuiTreeNodeFlags Flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_DrawLinesToNodes;

		if (Entry == SelectedPath)
			Flags |= ImGuiTreeNodeFlags_Selected;

		const bool bIsOpen =
			ImGui::TreeNodeEx(FilenameString.c_str(), Flags);

		if (ImGui::IsItemClicked())
			SelectedPath = Entry;

		if (bIsOpen)
		{
			RenderFolderTree(Entry, SelectedPath);
			ImGui::TreePop();
		}
	}

	if (IterateError)
	{
		ImGui::TextUnformatted("Could not read this folder.");
	}
}

void FContentDrawerPanel::DrawRenameDialog()
{
    const bool bOpening = bOpenRenameDialog;
    if (bOpening)
    {
        ImGui::OpenPopup("Rename Lua Script");
        bOpenRenameDialog = false;
    }
    if (!ImGui::BeginPopupModal("Rename Lua Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Name");
    if (bOpening) ImGui::SetKeyboardFocusHere();
    const bool bSubmit = ImGui::InputText("##LuaScriptName", RenameBuffer, sizeof(RenameBuffer),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    if (ImGui::Button("Rename") || bSubmit)
    {
        const fs::path Root = fs::u8path(FScriptEditorUtils::GetScriptRoot());
        const FString Relative = Utf8AssetPath(PendingRenamePath.lexically_relative(Root));
        FString NewRelative;
        if (FScriptEditorUtils::RenameScript(Relative, RenameBuffer, World, NewRelative, RenameError))
        {
            SelectAsset(Utf8AssetPath(Root / fs::u8path(NewRelative)));
            PendingRenamePath.clear();
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        PendingRenamePath.clear();
        ImGui::CloseCurrentPopup();
    }
    if (!RenameError.empty()) ImGui::TextWrapped("%s", RenameError.c_str());
    ImGui::EndPopup();
}
