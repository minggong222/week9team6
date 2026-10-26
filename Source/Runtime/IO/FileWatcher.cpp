#include "EnginePCH.h"
#include "FileWatcher.h"

void FFileWatcher::Watch(FStringView Path)
{
    const FString Key{ Path };
    if (Key.empty() || WatchedFiles.Contains(Key))
    {
        return;
    }

    FWatchedFile File;
    std::error_code Error;
    File.LastWriteTime = std::filesystem::last_write_time(std::filesystem::u8path(Key), Error);
    File.bHasWriteTime = !Error;
    WatchedFiles.Add(Key, File);
}

void FFileWatcher::Unwatch(FStringView Path)
{
    WatchedFiles.Remove(FString{ Path });
}

TArray<FString> FFileWatcher::PollChanges()
{
    TArray<FString> ChangedPaths;
    const auto Now = std::chrono::steady_clock::now();
    if (Now < NextPollTime)
    {
        return ChangedPaths;
    }

    NextPollTime = Now + std::chrono::milliseconds(250);

    for (auto& [Path, File] : WatchedFiles)
    {
        std::error_code Error;
        const auto WriteTime = std::filesystem::last_write_time(std::filesystem::u8path(Path), Error);
        if (Error)
        {
            // 임시 파일로 교체하는 저장 방식에서는 원본이 잠시 사라질 수 있습니다.
            File.bPendingChange = true;
            continue;
        }

        if (!File.bHasWriteTime || File.LastWriteTime != WriteTime)
        {
            File.LastWriteTime = WriteTime;
            File.bHasWriteTime = true;
            File.bPendingChange = true;
        }
        else if (File.bPendingChange)
        {
            ChangedPaths.Add(Path);
            File.bPendingChange = false;
        }
    }

    return ChangedPaths;
}
