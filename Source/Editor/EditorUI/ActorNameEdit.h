#pragma once

#include "GameFramework/Actor.h"
#include "Engine/Level.h"
#include <imgui.h>
#include <cstdio>

// Outliner와 Details가 같은 이름 검증·확정 규칙을 사용한다.
struct FActorNameEdit
{
    void Begin(AActor* Actor)
    {
        Target = Actor;
        TargetUUID = Actor->GetUUID();
        OriginalName = Actor->GetName();
        std::snprintf(Buffer, sizeof(Buffer), "%s", OriginalName.c_str());
        Error.clear();
        bEditing = false;
    }

    bool Commit(AActor* Actor)
    {
        FString Name = Buffer;
        const size_t First = Name.find_first_not_of(" \t\r\n");
        if (First == FString::npos)
        {
            Error = "Enter an actor name.";
            return false;
        }
        Name = Name.substr(First, Name.find_last_not_of(" \t\r\n") - First + 1);
        const FName NewName(Name);
        if (NewName.IsNone())
        {
            Error = "This actor name is reserved.";
            return false;
        }
        if (ULevel* Level = Actor->GetLevel())
        {
            for (AActor* Other : Level->GetActors())
            {
                if (Other && Other != Actor && Other->GetFName() == NewName)
                {
                    Error = "An actor with this name already exists in this level.";
                    return false;
                }
            }
        }
        Actor->SetName(NewName);
        OriginalName = Actor->GetName();
        std::snprintf(Buffer, sizeof(Buffer), "%s", OriginalName.c_str());
        Error.clear();
        return true;
    }

    bool DrawInput(AActor* Actor, const char* Label, bool bCommitOnDeactivate = true)
    {
        if (Target != Actor || TargetUUID != Actor->GetUUID() ||
            (!bEditing && OriginalName != Actor->GetName()))
            Begin(Actor);

        if (bFocusInput)
        {
            ImGui::SetKeyboardFocusHere();
            bFocusInput = false;
        }
        const bool bWasEditing = bEditing;
        const bool bEnter = ImGui::InputText(Label, Buffer, sizeof(Buffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        const bool bDeactivated = ImGui::IsItemDeactivatedAfterEdit();
        bEditing = ImGui::IsItemActive();
        if (ImGui::IsItemEdited()) Error.clear();

        bool bCommitted = false;
        if (bWasEditing && ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            std::snprintf(Buffer, sizeof(Buffer), "%s", OriginalName.c_str());
            Error.clear();
        }
        else if (bEnter || (bCommitOnDeactivate && bDeactivated))
            bCommitted = Commit(Actor);

        if (!Error.empty()) ImGui::TextWrapped("%s", Error.c_str());
        return bCommitted;
    }

    bool bFocusInput = false;

private:
    AActor* Target = nullptr;
    uint32 TargetUUID = 0;
    FString OriginalName;
    FString Error;
    char Buffer[256] = {};
    bool bEditing = false;
};
