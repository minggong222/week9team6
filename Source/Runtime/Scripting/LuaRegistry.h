#pragma once

#include "Containers/Map.h"
#include "Containers/StringView.h"
#include "Core/EngineString.h"

struct FLuaScriptSource
{
    FString Source;
    uint64 Version = 0;
};

// 실행 환경과 분리된 소스 저장소입니다. PIE를 종료해도 채택한 버전은 유지합니다.
class FLuaRegistry
{
public:
    const FLuaScriptSource* FindOrLoad(FStringView Path);

    // 파일 읽기와 문법 검사만 수행하며, 최상위 코드를 실행하거나 현재 버전을 바꾸지 않습니다.
    bool ReadCandidate(FStringView Path, FString& OutSource) const;

    // 검증에 성공한 후보만 전달합니다. PIE에서는 실행 검증까지 성공한 뒤 호출합니다.
    const FLuaScriptSource& Accept(FStringView Path, FString Source);
    bool Reload(FStringView Path);

private:
    TMap<FString, FLuaScriptSource> Scripts;
};
