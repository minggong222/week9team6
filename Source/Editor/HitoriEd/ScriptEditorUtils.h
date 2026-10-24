#pragma once

#include "Core/EngineString.h"

class AActor;
class ULuaComponent;
class UWorld;

class FScriptEditorUtils
{
public:
    static const FString& GetScriptRoot();

    // RelativePath is relative to Assets (e.g. Scripts/Test.lua).
    // Open an existing Lua file in its Windows-associated editor.
    static bool EditScript(const FString& RelativePath);

    // ScenePath is optional. Unsaved scenes use the Actor name alone.
    static bool CreateScript(ULuaComponent* Component, const FString& ScenePath);
    // Reuse an existing component, or attach one automatically on success.
    static ULuaComponent* CreateScriptForActor(AActor* Actor, const FString& ScenePath);

    // Input and output paths are relative to Assets.
    // Rename within the script's folder; update references in the open editor World.
    // Save the scene afterwards to persist those reference changes.
    static bool RenameScript(const FString& RelativePath, const FString& NewName,
        UWorld* World, FString& OutRelativePath, FString& OutError);
};
