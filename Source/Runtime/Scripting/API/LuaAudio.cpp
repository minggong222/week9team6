#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "IO/FileUtils.h"

#include <mmsystem.h>
#include <system_error>

namespace
{
	bool PlayBGM(const FString& AssetPath)
	{
		if (AssetPath.empty())
		{
			return false;
		}

		const FString FullPath = FileUtils::GetAssetFullPath(AssetPath);

		const std::filesystem::path NativePath = std::filesystem::u8path(FullPath.begin(), FullPath.end());

        std::error_code Error;
        if (!std::filesystem::is_regular_file(NativePath, Error))
        {
            HTR_LOG(Warning, "BGM file not found: {}", AssetPath);
            return false;
        }

        // WAV 파일을 비동기로 반복 재생한다.
        const BOOL bPlayed = PlaySoundW(NativePath.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_LOOP);

        if (!bPlayed)
        {
            HTR_LOG(Warning, "Failed to play BGM: {}", AssetPath);
            return false;
        }

        return true;
    }

    void StopBGM()
    {
        PlaySoundW(nullptr, nullptr, 0);
    }
}

void LuaRegisterAudio(sol::state& State)
{
    State.create_named_table("Audio", "PlayBGM", &PlayBGM, "StopBGM", &StopBGM);
}