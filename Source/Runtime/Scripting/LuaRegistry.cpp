#include "EnginePCH.h"
#include "LuaRegistry.h"

#include "IO/FileUtils.h"
#include "sol/sol.hpp"
#include <fstream>
#include <iterator>

const FLuaScriptSource* FLuaRegistry::FindOrLoad(FStringView Path)
{
    if (Path.empty())
    {
        return nullptr;
    }

    const FString Key = FileUtils::GetAssetFullPath(Path);
    if (const FLuaScriptSource* Script = Scripts.Find(Key))
    {
        return Script;
    }

    FString Source;
    if (!ReadCandidate(Path, Source))
    {
        return nullptr;
    }

    return &Accept(Path, std::move(Source));
}

bool FLuaRegistry::ReadCandidate(FStringView Path, FString& OutSource) const
{
    if (Path.empty())
    {
        return false;
    }

    const FString FullPath = FileUtils::GetAssetFullPath(Path);
    std::ifstream File(std::filesystem::u8path(FullPath), std::ios::binary);
    if (!File)
    {
        HTR_LOG(Error, "Lua 소스 읽기 실패: {}", FullPath);
        return false;
    }

    FString Source{ std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>() };
    if (File.bad())
    {
        HTR_LOG(Error, "Lua 소스 읽기 실패: {}", FullPath);
        return false;
    }

    // 검사용 State에는 게임 바인딩이나 실행 환경을 만들지 않습니다.
    sol::state ValidationState;
    sol::load_result Result = ValidationState.load(Source, "@" + FullPath);
    if (!Result.valid())
    {
        sol::error Error = Result;
        HTR_LOG(Error, "Lua 문법 검사 실패: {}", Error.what());
        return false;
    }

    OutSource = std::move(Source);
    return true;
}

const FLuaScriptSource& FLuaRegistry::Accept(FStringView Path, FString Source)
{
    FLuaScriptSource& Script = Scripts[FileUtils::GetAssetFullPath(Path)];
    if (Script.Version == 0 || Script.Source != Source)
    {
        Script.Source = std::move(Source);
        ++Script.Version;
    }

    return Script;
}

bool FLuaRegistry::Reload(FStringView Path)
{
    FString Source;
    if (!ReadCandidate(Path, Source))
    {
        return false;
    }

    const FLuaScriptSource& Script = Accept(Path, std::move(Source));
    HTR_LOG(Info, "Lua 소스 갱신: {} (버전 {})", Path, Script.Version);
    return true;
}
