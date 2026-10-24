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

	Instance->Unload();
	Instance->OnLoaded.Clear();
	Instance->SetOwner(nullptr);
}

void ULuaComponent::Initialize()
{
	FLuaManager& Manager = *GetWorld()->GetLuaManager();
	Instance = Manager.CreateInstance(ScriptPath, this);

	Instance->OnLoaded.AddDynamic(this, &ULuaComponent::OnInstanceLoaded);
}

bool ULuaComponent::Load()
{
	if (!Instance)
	{
		return false;
	}

	if (!Instance->Load(ScriptPath))
	{
		Instance->Unload();
		return false;
	}

	return true;
}

void ULuaComponent::OnInstanceLoaded(FLuaInstance& Instance)
{
	Instance.SetGlobalVariable("Actor", GetOwner());
}

void ULuaComponent::InitializeComponent()
{
	if (!Instance)
	{
		return;
	}

	Instance->Invoke("InitializeComponent", this);
}

void ULuaComponent::BeginPlay()
{
	if (!Instance)
	{
		return;
	}

	Instance->Invoke("BeginPlay", this);
}

void ULuaComponent::TickComponent(float DeltaSeconds)
{
	if (!Instance)
	{
		return;
	}

	Instance->Invoke("TickComponent", this, DeltaSeconds);
}

/*
void ULuaComponent::EndPlay()
{
	if (!Instance)
	{
		return;
	}

	Instance->Invoke("EndPlay", this);
}
*/
