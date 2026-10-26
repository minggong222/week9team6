#include "EnginePCH.h"
#include "Editor/Outliner/OutlinerPanel.h"

#include "Engine/Level.h"
#include "Engine/World.h"

#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include <algorithm>
#include <cctype>

namespace
{
    FString FoldName(FString Name)
    {
        for (char& C : Name) C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
        return Name;
    }

    // 숫자를 정수로 변환하지 않아 긴 숫자에서도 오버플로가 나지 않는다.
    bool NaturalNameLess(const FString& Left, const FString& Right)
    {
        size_t L = 0, R = 0;
        const auto IsDigit = [](char C) { return C >= '0' && C <= '9'; };
        while (L < Left.size() && R < Right.size())
        {
            if (IsDigit(Left[L]) && IsDigit(Right[R]))
            {
                size_t LEnd = L, REnd = R;
                while (LEnd < Left.size() && IsDigit(Left[LEnd])) ++LEnd;
                while (REnd < Right.size() && IsDigit(Right[REnd])) ++REnd;
                size_t LStart = L, RStart = R;
                while (LStart < LEnd && Left[LStart] == '0') ++LStart;
                while (RStart < REnd && Right[RStart] == '0') ++RStart;
                if (LEnd - LStart != REnd - RStart) return LEnd - LStart < REnd - RStart;
                const int Digits = Left.compare(LStart, LEnd - LStart, Right, RStart, REnd - RStart);
                if (Digits != 0) return Digits < 0;
                L = LEnd;
                R = REnd;
                continue;
            }
            const int LC = std::tolower(static_cast<unsigned char>(Left[L]));
            const int RC = std::tolower(static_cast<unsigned char>(Right[R]));
            if (LC != RC) return LC < RC;
            ++L;
            ++R;
        }
        if (L != Left.size() || R != Right.size()) return L == Left.size();
        return Left < Right;
    }
}

bool FOutlinerPanel::Init()
{
    return true;
}

void FOutlinerPanel::Tick(float DeltaTime)
{
}

void FOutlinerPanel::OnRender()
{
    // World 참조 확인
    if (!World)
        return;

    // 현재 Level 가져오기
    ULevel* Level = World->GetCurrentLevel();
    if (!Level)
        return;

    // Level의 Actor 목록 Tree 출력
    if (ImGui::Begin("Outliner"))
    {
        const float ButtonSize = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(-ButtonSize - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::InputTextWithHint("##ActorSearch", "Search actors...", SearchText, sizeof(SearchText)))
            bFilterDirty = true;
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(ButtonSize, ButtonSize)))
        {
            SearchText[0] = '\0';
            bFilterDirty = true;
        }

        DrawActors(Level);
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F2, false))
        {
            if (AActor* Selected = GetSelectedActor()) BeginRename(Selected);
        }
        DrawRenamePopup(Level);
    }

    ImGui::End();

    // Actors 순회가 완전히 끝난 뒤 삭제
    if (PendingDeleteActor)
    {
        if (DeleteCallback)
        {
            DeleteCallback(PendingDeleteActor);
        }

        PendingDeleteActor = nullptr;
    }
}

// Level이 가지고 있는 Actor 목록 순회하며 Draw TreeNode
void FOutlinerPanel::DrawActors(ULevel* Level)
{
    if (!Level)
        return;

    if (CachedLevel != Level || CachedActorListRevision != Level->GetActorListRevision())
        bActorListDirty = true;

    if (bActorListDirty)
    {
        SortedActors.clear();
        SortedActors.reserve(Level->GetActorNum());
        for (AActor* Actor : Level->GetActors())
            if (Actor) SortedActors.push_back({ Actor, Actor->GetName() });
        std::sort(SortedActors.begin(), SortedActors.end(), [](const FActorEntry& A, const FActorEntry& B)
        {
            if (A.Name == B.Name) return A.Actor->GetUUID() < B.Actor->GetUUID();
            return NaturalNameLess(A.Name, B.Name);
        });
        CachedLevel = Level;
        CachedActorListRevision = Level->GetActorListRevision();
        bActorListDirty = false;
        bFilterDirty = true;

        if (SelectedObject && std::none_of(SortedActors.begin(), SortedActors.end(),
            [this](const FActorEntry& Entry) { return Entry.Actor == SelectedObject; }))
            SelectActor(nullptr);
    }

    if (bFilterDirty)
    {
        FilteredActorIndices.clear();
        const FString Search = FoldName(SearchText);
        for (size_t Index = 0; Index < SortedActors.size(); ++Index)
            if (Search.empty() || FoldName(SortedActors[Index].Name).find(Search) != FString::npos)
                FilteredActorIndices.push_back(Index);
        bFilterDirty = false;
    }
    ImGui::TextDisabled("%d / %d actors", static_cast<int>(FilteredActorIndices.size()), static_cast<int>(SortedActors.size()));
    if (FilteredActorIndices.empty()) ImGui::TextDisabled("No actors match.");

    ImGuiListClipper Clipper;
	Clipper.Begin(static_cast<int32>(FilteredActorIndices.size()), ImGui::GetTextLineHeightWithSpacing());

    while (Clipper.Step())
    {
		for (int32 Index = Clipper.DisplayStart; Index < Clipper.DisplayEnd; ++Index)
		{
			AActor* Actor = SortedActors[FilteredActorIndices[Index]].Actor;
			if (Actor)
				DrawActorNode(Actor);
		}
    }

}

// Actor를 ImGui TreeNode로 출력
void FOutlinerPanel::DrawActorNode(AActor* Actor)
{
    if (!Actor)
        return;

    const FString& ActorName = Actor->GetName();

    //ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_None;
    ImGuiTreeNodeFlags Flags = 
        ImGuiTreeNodeFlags_Leaf |
        ImGuiTreeNodeFlags_NoTreePushOnOpen |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    // Actor 선택 시 UI 선택 상태
    if (SelectedObject == Actor)
        Flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::TreeNodeEx(Actor, Flags, "%s", ActorName.c_str());

    // UI에서 Actor 클릭
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        SelectActor(Actor);
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Rename", "F2")) BeginRename(Actor);
        if (ImGui::MenuItem("Delete"))
        {
            PendingDeleteActor = Actor; // 여기서는 바로 삭제 X
        }

        ImGui::EndPopup();
    }
    
}

void FOutlinerPanel::BeginRename(AActor* Actor)
{
    if (!Actor) return;
    SelectActor(Actor);
    RenameActor = Actor;
    RenameActorUUID = Actor->GetUUID();
    RenameEdit.Begin(Actor);
    RenameEdit.bFocusInput = true;
    bOpenRenamePopup = true;
}

void FOutlinerPanel::DrawRenamePopup(ULevel* Level)
{
    if (bOpenRenamePopup)
    {
        ImGui::OpenPopup("Rename Actor");
        bOpenRenamePopup = false;
    }
    if (!ImGui::BeginPopupModal("Rename Actor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    // 삭제되거나 주소가 재사용된 액터를 편집하지 않는다.
    bool bValidActor = false;
    for (AActor* Actor : Level->GetActors())
        if (Actor && Actor == RenameActor && Actor->GetUUID() == RenameActorUUID) { bValidActor = true; break; }
    if (!bValidActor)
    {
        RenameActor = nullptr;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    ImGui::SetNextItemWidth(300.0f);
    bool bClose = RenameEdit.DrawInput(RenameActor, "Name", false);
    if (ImGui::Button("Rename")) bClose = RenameEdit.Commit(RenameActor);
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) bClose = true;
    if (bClose)
    {
        RenameActor = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// 선택 Object 변경
void FOutlinerPanel::SelectActor(AActor* Actor)
{
    if (!Actor)
    {
        SelectedObject = nullptr;

        if (Callback)
            Callback(nullptr);

        return;
    }

    SelectedObject = Actor;

    HTR_LOG(Info, "{} UUID {} is selected", SelectedObject->GetName(), SelectedObject->GetUUID());

    USceneComponent* Scene = Cast<USceneComponent>(Actor->GetRootComponent());

    if (Callback)
        Callback(Scene);
}


AActor* FOutlinerPanel::GetSelectedActor() const
{
    return Cast<AActor>(SelectedObject);
}
