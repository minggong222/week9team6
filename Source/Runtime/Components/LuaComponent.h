#pragma once

#include "ActorComponent.h"
#include "Core/EngineString.h"

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

    void OnInstanceLoaded(FLuaInstance& Instance);

    virtual void InitializeComponent() override;
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaSeconds) override;
    virtual void EndPlay() override;

    const FString& GetScriptPath() const
    {
        return ScriptPath;
    }

    bool SetScriptPath(const FString& InPath)
    {
        // 경로 지정은 실행하지 않습니다. 실제 로드는 InitializeComponent에서 수행합니다.
        ScriptPath = InPath;
        return true;
    }

    bool GetInitialized() const;
    bool GetLoaded() const;

private:
    // Assets 기준 상대 경로입니다. 예: Scripts/Test.lua
    FString ScriptPath;

    FLuaInstance* Instance = nullptr;
    // 비소유 참조: GameInstance의 매니저는 컴포넌트보다 오래 살아 있어야 합니다.
    FLuaManager* InstanceManager = nullptr;
};
