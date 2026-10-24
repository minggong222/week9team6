#pragma once

#include "Scripting/LuaInstance.h"

class ULuaComponent;

// Manager-owned record: FLuaInstance handles execution, this record adds the owner link.
class FScriptInstance
{
public:
    FScriptInstance(sol::state& InState, ULuaComponent* InOwnerComponent)
        : Script(InState), OwnerComponent(InOwnerComponent)
    {
    }

    FScriptInstance(const FScriptInstance&) = delete;
    FScriptInstance& operator=(const FScriptInstance&) = delete;

    FLuaInstance& GetScript() { return Script; }
    const FLuaInstance& GetScript() const { return Script; }
    const FString& GetLoadedPath() const { return Script.GetPath(); }
    ULuaComponent* GetOwnerComponent() const { return OwnerComponent; }

private:
    FLuaInstance Script;
    ULuaComponent* OwnerComponent = nullptr;
};
