#pragma once

#include "ActorComponent.h"
#include "Core/EngineString.h"

class FLuaManager;

class ULuaComponent : public UActorComponent
{
    DECLARE_CLASS(ULuaComponent, UActorComponent)

    REFLECT_START(ClassName)
        PROPERTY_FLAGS(ScriptPath, CPF_VisibleAnywhere)
    REFLECT_END()

public:
    ULuaComponent();
    virtual ~ULuaComponent() override;

    // Runtime operation implementations will be connected in the next step.
    bool Load(FLuaManager& InManager);
    bool Invoke(const FString& FunctionName);
    bool Invoke(const FString& FunctionName, float Argument);
    bool Reload();
    void Destroy();

    const FString& GetScriptPath() const
    {
        return ScriptPath;
    }

    bool SetScriptPath(const FString& InPath)
    {
        if (InstanceIndex != -1)
        {
            return false;
        }

        ScriptPath = InPath;
        return true;
    }

    int32 GetInstanceIndex() const
    {
        return InstanceIndex;
    }

private:
    // Path relative to Assets/Scripts, assigned by Create Script and saved with the scene.
    FString ScriptPath;

    // Runtime-only data. The manager must outlive the script instance.
    FLuaManager* LuaManager = nullptr;
    int32 InstanceIndex = -1;
};
