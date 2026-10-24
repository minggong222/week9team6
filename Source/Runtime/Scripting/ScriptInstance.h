#pragma once

#include "Core/EngineString.h"
#include <sol/sol.hpp>

class ULuaComponent;

// One component's Lua execution environment. Owned by FLuaManager.
class FScriptInstance
{
public:
    FScriptInstance(sol::state& InState, ULuaComponent* InOwnerComponent);
    ~FScriptInstance() = default;

    FScriptInstance(const FScriptInstance&) = delete;
    FScriptInstance& operator=(const FScriptInstance&) = delete;

    bool Load(const FString& Path);
    bool Invoke(const FString& FunctionName);
    bool Invoke(const FString& FunctionName, float Argument);
    bool Reload();

    const FString& GetLoadedPath() const
    {
        return LoadedPath;
    }

    ULuaComponent* GetOwnerComponent() const
    {
        return OwnerComponent;
    }

private:
    // Non-owning references. Both must remain valid while this instance executes.
    sol::state& State;
    ULuaComponent* OwnerComponent = nullptr;

    sol::environment Environment;

    // Update only after a successful load. Reload uses this path.
    FString LoadedPath;
};
