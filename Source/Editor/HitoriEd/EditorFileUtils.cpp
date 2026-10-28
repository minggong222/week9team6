#include "EnginePCH.h"
#include "Editor/HitoriEd/EditorFileUtils.h"

#include "Core/EngineLog.h"
#include "EditorAssetFileDialog.h"
#include "Serialization/JsonArchive.h"
 
FString FEditorFileUtils::CurrentScenePath = "";

namespace
{
	std::filesystem::path& SceneDialogFolder()
	{
		static auto Folder = std::filesystem::absolute("Scenes").lexically_normal();
		return Folder;
	}
}

bool FEditorFileUtils::NewScene(UWorld* World)
{
	if (!World)
		return false;

	World->ClearWorld();

	// 라인 트래커도 삭제
	World->GetPathTracker().ClearPath();

	CurrentScenePath.clear();

	return true;
}

bool FEditorFileUtils::SaveScene(UWorld* World)
{
	if (!World)
		return false;

	if (CurrentScenePath.empty())
	{
		return SaveSceneAs(World);
	}

	if (!FJsonArchive::SaveWorld(World, CurrentScenePath))
	{
		HTR_LOG(Error, "Failed to save scene: {}", CurrentScenePath);

		MessageBoxW(
			GetActiveWindow(),
			L"씬을 저장하지 못했습니다.\n경로와 쓰기 권한을 확인하세요.",
			L"저장 실패",
			MB_OK | MB_ICONERROR
		);
		return false;
	}
	HTR_LOG(Info, "Save Scene : {}", CurrentScenePath);

	return true;
}

bool FEditorFileUtils::SaveSceneAs(UWorld* World)
{
	if (!World)
		return false;

	FString FilePath = OpenSaveSceneDialog();

	if (FilePath.empty())
		return false;

	FilePath = std::filesystem::absolute(FilePath).lexically_normal().string();

	if (!FJsonArchive::SaveWorld(World, FilePath))
	{
		HTR_LOG(Error, "Failed to save scene: {}", FilePath);

		MessageBoxW(
			GetActiveWindow(),
			L"씬을 저장하지 못했습니다.\n경로와 쓰기 권한을 확인하세요.",
			L"저장 실패",
			MB_OK | MB_ICONERROR
		);

		return false;
	}

	CurrentScenePath = FilePath;

	HTR_LOG(Info, "Save Scene As : {}", CurrentScenePath);

	return true;
}

bool FEditorFileUtils::LoadScene(UWorld* World)
{
	if (!World)
		return false;

	FString FilePath = OpenLoadSceneDialog();

	// 파일 선택 취소
	if (FilePath.empty())
		return false;

	FilePath = std::filesystem::absolute(FilePath).lexically_normal().string();

	if (!FJsonArchive::LoadWorld(World, FilePath))
		return false;

	// 이제 이 파일이 현재 Scene
	CurrentScenePath = FilePath;

	HTR_LOG(Info, "Load Scene : {}", CurrentScenePath);

	return true;
}

FString FEditorFileUtils::OpenSaveSceneDialog()
{
    std::error_code Error;
    std::filesystem::create_directories(SceneDialogFolder(), Error);
    if (Error) return FString();
    std::filesystem::path Path;
    const HRESULT Status = EditorAssetFileDialog::Show(true, EditorAssetFileDialog::SceneId,
        SceneDialogFolder(), L"Scene Files (*.scene)", L"*.scene", L"scene", L"Untitled.scene", Path);
    if (FAILED(Status)) return FString();
    SceneDialogFolder() = Path.parent_path();
    return Path.string();
}

FString FEditorFileUtils::OpenLoadSceneDialog()
{
    std::error_code Error;
    std::filesystem::create_directories(SceneDialogFolder(), Error);
    if (Error) return FString();
    std::filesystem::path Path;
    const HRESULT Status = EditorAssetFileDialog::Show(false, EditorAssetFileDialog::SceneId,
        SceneDialogFolder(), L"Scene Files (*.scene)", L"*.scene", L"scene", L"", Path);
    if (FAILED(Status)) return FString();
    SceneDialogFolder() = Path.parent_path();
    return Path.string();
}