#include "EnginePCH.h"
#include "Editor/HitoriEd/ScriptEditorUtils.h"

#include "Components/LuaComponent.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Core/EngineLog.h"
#include <filesystem>
#include <vector>
#include <cctype>
#include <shellapi.h>

#pragma comment(lib, "Shell32.lib")

namespace
{
    FString ToUtf8(const std::filesystem::path& Path)
    {
        const auto Text = Path.generic_u8string();
        return FString(Text.begin(), Text.end());
    }

    bool IsValidName(const FString& Name)
    {
        if (Name.empty() || Name == "." || Name == ".." ||
            Name.back() == '.' || Name.back() == ' ') return false;
        for (unsigned char Character : Name)
            if (Character < 32 || FString("<>:\"/\\|?*").find(Character) != FString::npos)
                return false;
        FString DeviceName = Name.substr(0, Name.find('.'));
        for (char& C : DeviceName) C = static_cast<char>(std::toupper(static_cast<unsigned char>(C)));
        if (DeviceName == "CON" || DeviceName == "PRN" || DeviceName == "AUX" || DeviceName == "NUL")
            return false;
        if (DeviceName.size() == 4 && (DeviceName.starts_with("COM") || DeviceName.starts_with("LPT")) &&
            DeviceName[3] >= '1' && DeviceName[3] <= '9') return false;
        return true;
    }

    FString MakeDefaultName(FString Name)
    {
        for (char& C : Name)
            if (static_cast<unsigned char>(C) < 32 || FString("<>:\"/\\|?*").find(C) != FString::npos) C = '_';
        while (!Name.empty() && (Name.back() == '.' || Name.back() == ' ')) Name.pop_back();
        if (Name.empty()) Name = "Script";
        if (!IsValidName(Name)) Name = "Script_" + Name;
        return Name;
    }
}

const FString& FScriptEditorUtils::GetScriptRoot()
{
    static const FString Root = "Assets/Scripts";
    return Root;
}

bool FScriptEditorUtils::CreateScript(ULuaComponent* Component, const FString& ScenePath)
{
    if (!Component || !Component->GetOwner()) return false;
    const UWorld* World = Component->GetOwner()->GetWorld();
    if (Component->GetInstanceIndex() != -1 ||
        (World && (World->HasBegunPlay() || World->GetWorldType() != EWorldType::Editor)))
    {
        HTR_LOG(Warning, "Create Script: stop play before creating a script.");
        return false;
    }
    if (!Component->GetScriptPath().empty()) return false;

    namespace Files = std::filesystem;
    try
    {
        const Files::path Directory = Files::u8path(GetScriptRoot());
        const Files::path Template = Directory / "template.lua";
        if (!Files::is_regular_file(Template))
        {
            HTR_LOG(Error, "Create Script: missing template: {}", ToUtf8(Template));
            return false;
        }

        FString BaseName = Component->GetOwner()->GetName();
        if (!ScenePath.empty())
            BaseName = ToUtf8(Files::u8path(ScenePath).stem()) + "_" + BaseName;
        BaseName = MakeDefaultName(BaseName);

        Files::path Destination = Directory / Files::u8path(BaseName + ".lua");
        for (int Suffix = 1; Files::exists(Destination); ++Suffix)
            Destination = Directory / Files::u8path(BaseName + "_" + std::to_string(Suffix) + ".lua");

        const FString RelativePath = ToUtf8(Destination.filename());
        Files::copy_file(Template, Destination, Files::copy_options::none);
        if (!Component->SetScriptPath(RelativePath))
        {
            std::error_code Error;
            Files::remove(Destination, Error);
            HTR_LOG(Error, "Create Script: could not assign {} to the component.", RelativePath);
            return false;
        }
        HTR_LOG(Info, "Created script: {}. Save the scene to keep the Actor connection.", RelativePath);
        return true;
    }
    catch (const Files::filesystem_error& Error)
    {
        HTR_LOG(Error, "Create Script failed: {}", Error.what());
        return false;
    }
}

ULuaComponent* FScriptEditorUtils::CreateScriptForActor(AActor* Actor, const FString& ScenePath)
{
    if (!Actor) return nullptr;
    ULuaComponent* Component = nullptr;
    for (UActorComponent* Candidate : Actor->GetComponents())
        if ((Component = Cast<ULuaComponent>(Candidate))) break;

    if (Component && !Component->GetScriptPath().empty()) return Component;
    const bool bCreatedComponent = !Component;
    if (bCreatedComponent)
        Component = Cast<ULuaComponent>(Actor->AddComponent(ULuaComponent::StaticClass(), FName()));
    if (Component && CreateScript(Component, ScenePath)) return Component;
    if (bCreatedComponent && Component) Actor->DestroyComponent(Component);
    return nullptr;
}

bool FScriptEditorUtils::RenameScript(const FString& RelativePath, const FString& NewName,
    UWorld* World, FString& OutRelativePath, FString& OutError)
{
    OutRelativePath.clear();
    OutError.clear();
    if (!World || World->GetWorldType() != EWorldType::Editor || World->HasBegunPlay())
    {
        OutError = "Rename scripts outside play mode.";
        return false;
    }

    namespace Files = std::filesystem;
    try
    {
        const Files::path Root = Files::weakly_canonical(Files::u8path(GetScriptRoot()));
        const Files::path OldPath = Files::weakly_canonical(Root / Files::u8path(RelativePath));
        const Files::path OldRelative = OldPath.lexically_relative(Root);
        if (OldRelative.empty() || OldRelative.is_absolute() || *OldRelative.begin() == ".." ||
            OldPath.extension() != ".lua" || OldRelative == "template.lua" || !Files::is_regular_file(OldPath))
        {
            OutError = "Choose a Lua script inside Assets/Scripts. The template cannot be renamed.";
            return false;
        }

        const FString FileName = NewName.ends_with(".lua") ? NewName : NewName + ".lua";
        if (!IsValidName(NewName) || !IsValidName(FileName) || FileName == ".lua")
        {
            OutError = "Enter a valid Lua filename.";
            return false;
        }
        const Files::path NewPath = OldPath.parent_path() / Files::u8path(FileName);
        const FString OldReference = ToUtf8(OldRelative);
        const FString NewReference = ToUtf8(NewPath.lexically_relative(Root));
        if (OldPath == NewPath) { OutRelativePath = OldReference; return true; }
        if (Files::exists(NewPath)) { OutError = "A file with that name already exists."; return false; }

        std::vector<ULuaComponent*> References;
        for (ULevel* Level : World->GetLevels())
            if (Level)
                for (AActor* Actor : Level->GetActors())
                    if (Actor)
                        for (UActorComponent* Component : Actor->GetComponents())
                            if (ULuaComponent* Script = Cast<ULuaComponent>(Component))
                                if (Script->GetScriptPath() == OldReference)
                                {
                                    if (Script->GetInstanceIndex() != -1)
                                    {
                                        OutError = "Stop the script before renaming it.";
                                        return false;
                                    }
                                    References.push_back(Script);
                                }

        Files::rename(OldPath, NewPath);
        for (ULuaComponent* Script : References) Script->SetScriptPath(NewReference);
        OutRelativePath = NewReference;
        HTR_LOG(Info, "Renamed script: {} -> {}. Save the scene to keep reference changes.", OldReference, NewReference);
        return true;
    }
    catch (const Files::filesystem_error& Error)
    {
        OutError = Error.what();
        return false;
    }
}


bool FScriptEditorUtils::EditScript(const FString& RelativePath)
{
    namespace Files = std::filesystem;

    try
    {
        const Files::path ScriptPath = Files::absolute(
            Files::u8path(GetScriptRoot()) / Files::u8path(RelativePath));

        if (ScriptPath.extension() != ".lua" || !Files::is_regular_file(ScriptPath))
        {
            MessageBoxW(nullptr, L"Lua 파일을 찾을 수 없습니다.",
                L"Edit Script", MB_OK | MB_ICONERROR);
            return false;
        }

        const HINSTANCE Result = ShellExecuteW(
            nullptr,
            L"open",
            ScriptPath.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL);

        if (reinterpret_cast<INT_PTR>(Result) <= 32)
        {
            MessageBoxW(nullptr,
                L"파일을 열지 못했습니다.\n.lua 연결 프로그램을 확인하세요.",
                L"Edit Script", MB_OK | MB_ICONERROR);
            return false;
        }

        return true;
    }
    catch (const Files::filesystem_error& Error)
    {
        HTR_LOG(Error, "Edit Script failed: {}", Error.what());
        return false;
    }
}
