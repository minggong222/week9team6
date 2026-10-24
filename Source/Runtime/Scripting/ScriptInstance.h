#pragma once

#include "Scripting/Lua.h"

class ULuaComponent;

// Manager-owned record: FLua handles execution, this record adds the owner link.
class FScriptInstance
{
public:
    FScriptInstance(sol::state& InState, ULuaComponent* InOwnerComponent)
        : Script(InState), OwnerComponent(InOwnerComponent)
    {
    }

    FScriptInstance(const FScriptInstance&) = delete;
    FScriptInstance& operator=(const FScriptInstance&) = delete;

    FLua& GetScript() { return Script; }
    const FLua& GetScript() const { return Script; }
    const FString& GetLoadedPath() const { return Script.GetPath(); }
    ULuaComponent* GetOwnerComponent() const { return OwnerComponent; }

private:
    FLua Script;
    ULuaComponent* OwnerComponent = nullptr;
};
