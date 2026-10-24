#include "EnginePCH.h"
#include "FileUtils.h"

#include <filesystem>

namespace FileUtils
{
    FString GetAssetFullPath(FStringView AssetPath)
    {
        const std::filesystem::path Path =
            std::filesystem::absolute(std::filesystem::path("Assets") /
                std::filesystem::u8path(AssetPath.begin(), AssetPath.end())).lexically_normal();
        const auto Utf8Path = Path.generic_u8string();
        return FString(Utf8Path.begin(), Utf8Path.end());
    }
}
