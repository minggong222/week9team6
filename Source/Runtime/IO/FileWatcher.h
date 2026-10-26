#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringView.h"
#include "Core/EngineString.h"

#include <chrono>
#include <filesystem>

class FFileWatcher
{
public:
    // 경로는 정규화된 UTF-8 절대 경로를 전달합니다. 중복 등록은 무시합니다.
    void Watch(FStringView Path);
    void Unwatch(FStringView Path);

    // 250ms 간격으로 검사하고, 한 검사 주기 동안 안정된 변경만 반환합니다.
    TArray<FString> PollChanges();

private:
    struct FWatchedFile
    {
        std::filesystem::file_time_type LastWriteTime{};
        bool bHasWriteTime = false;
        bool bPendingChange = false;
    };

    TMap<FString, FWatchedFile> WatchedFiles;
    std::chrono::steady_clock::time_point NextPollTime{};
};
