#include "EnginePCH.h"
#include "LuaComponent.h"

ULuaComponent::~ULuaComponent()
{
	if (!Instance)
	{
		return;
	}

	Instance->Destroy();
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
