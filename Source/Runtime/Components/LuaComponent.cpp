#include "EnginePCH.h"
#include "LuaComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Scripting/LuaInstance.h"


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

void ULuaComponent::InitializeLua()
{
	FLuaManager& Manager = *GetWorld()->GetLuaManager();
	Instance = Manager.CreateInstance(ScriptPath, this);

	Instance->OnLoaded.AddDynamic(this, &ULuaComponent::OnInstanceLoaded);
}

bool ULuaComponent::LoadLua()
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
	Super::InitializeComponent();

	// 인스턴스 초기화
	InitializeLua();
	LoadLua();

	if (GetLoaded())
	{
		Instance->Invoke("InitializeComponent", this);
	}
}

void ULuaComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!Instance)
	{
		return;
	}

	if (!GetLoaded())
	{
		return;
	}

	Instance->Invoke("BeginPlay", this);
}

void ULuaComponent::TickComponent(float DeltaSeconds)
{
	Super::TickComponent(DeltaSeconds);

	if (!GetLoaded())
	{
		return;
	}

	Instance->Invoke("TickComponent", this, DeltaSeconds);
}

void ULuaComponent::EndPlay()
{
	Super::EndPlay();

	if (!GetLoaded())
	{
		return;
	}

	Instance->Invoke("EndPlay", this);
}

bool ULuaComponent::GetInitialized() const
{
	return Instance != nullptr;
}

bool ULuaComponent::GetLoaded() const
{
	return GetInitialized() && Instance->GetInitialized();
}

