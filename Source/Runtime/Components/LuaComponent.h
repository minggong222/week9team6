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
    virtual ~ULuaComponent() override;

    // Runtime operation implementations will be connected in the next step.
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
        ScriptPath = InPath;
        return LoadLua();
    }

    bool GetInitialized() const;
    bool GetLoaded() const;

private:
    FString ScriptPath;

    FLuaInstance* Instance = nullptr;
};
