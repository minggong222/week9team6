#pragma once

#include "Core/EngineString.h"
#include "Containers/StringView.h"

namespace FileUtils
{
    // Assets 기준 상대 경로를 UTF-8 절대 경로로 변환합니다.
    FString GetAssetFullPath(FStringView AssetPath);
}
