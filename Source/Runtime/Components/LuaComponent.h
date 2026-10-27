#pragma once

#include "ActorComponent.h"
#include "Core/EngineString.h"
#include "sol/forward.hpp"
#include "Containers/StringView.h"

class FLuaManager;
class FLuaInstance;

class ULuaComponent : public UActorComponent
{
    DECLARE_CLASS(ULuaComponent, UActorComponent)

    REFLECT_START(ClassName)
        PROPERTY_FLAGS(ScriptPath, CPF_VisibleAnywhere)
    REFLECT_END()

public:
    ULuaComponent();
    virtual ~ULuaComponent() override;

    // 플레이 중 사용할 Lua 인스턴스를 초기화하고 스크립트를 로드합니다.
    void InitializeLua();
    bool LoadLua();
    bool ReloadLua();

    // 인자는 그대로 전달하며 Self를 자동으로 추가하지 않습니다.
    sol::variadic_results Invoke(const FString& FunctionName, sol::variadic_args Args);

    void OnInstanceLoaded(FLuaInstance& Instance);

    virtual void InitializeComponent() override;
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaSeconds) override;
    virtual void EndPlay() override;

    using Super::Serialize;
    virtual void Serialize(FStructuredArchive::FRecord Record) override;

    const FString& GetScriptPath() const
    {
        return ScriptPath;
    }

    bool SetScriptPath(const FString& InPath);

    bool GetInitialized() const;
    bool GetLoaded() const;

private:
    template<typename... Args>
    bool InvokeLua(FStringView Name, Args... Arguments);

    // Assets 기준 상대 경로입니다. 예: Scripts/Test.lua
    FString ScriptPath;

    FLuaInstance* Instance = nullptr;
    // 비소유 참조: GameInstance의 매니저는 컴포넌트보다 오래 살아 있어야 합니다.
    FLuaManager* InstanceManager = nullptr;
};
