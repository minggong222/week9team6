#include "EnginePCH.h"
#include "LuaComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"


ULuaComponent::~ULuaComponent()
{
	if (!Instance)
	{
		return;
	}

	Instance->Destroy();
}

bool ULuaComponent::Load()
{
	FLuaManager& Manager = *GetWorld()->GetLuaManager();
	FLuaInstance* Item = Manager.CreateInstance(ScriptPath, this);

	if (!Item)
	{
		// 실패
		return false;
	}

	Instance = Item;
	return true;
}

void ULuaComponent::InitializeComponent()
{
	if (!Instance)
	{
		return;
	}

	// TODO: Self를 넘길 수 있도록 수정
	Instance->Invoke("InitializeComponent");
}

void ULuaComponent::BeginPlay()
{
	if (!Instance)
	{
		return;
	}

	// TODO: Self를 넘길 수 있도록 수정
	Instance->Invoke("BeginPlay");
}

void ULuaComponent::TickComponent(float DeltaSeconds)
{
	if (!Instance)
	{
		return;
	}

	// TODO: Self를 넘길 수 있도록 수정
	Instance->Invoke("TickComponent", DeltaSeconds);
}

/*
void ULuaComponent::EndPlay()
{
	if (!Instance)
	{
		return;
	}

	// TODO: Self를 넘길 수 있도록 수정
	Instance->Invoke("EndPlay");
}
*/
