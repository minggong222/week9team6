#include "EnginePCH.h"
#include "LuaComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Scripting/LuaInstance.h"

ULuaComponent::ULuaComponent()
{
	// 스크립트가 동작해야 하므로 Tick을 활성화
	PrimaryComponentTick.bCanEverTick = true;
}

ULuaComponent::~ULuaComponent()
{
	if (!Instance)
	{
		return;
	}

	Instance->OnLoaded.Clear();
	Instance->SetOwner(nullptr);
	InstanceManager->Remove(Instance);
	Instance = nullptr;
	InstanceManager = nullptr;
}

void ULuaComponent::InitializeLua()
{
	if (GetInitialized())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FLuaManager* Manager = World->GetLuaManager();
	if (!Manager)
	{
		return;
	}

	Instance = Manager->CreateInstance(ScriptPath, this);
	InstanceManager = Manager;

	Instance->OnLoaded.AddDynamic(this, &ULuaComponent::OnInstanceLoaded);
}

bool ULuaComponent::LoadLua()
{
	if (!Instance || ScriptPath.empty())
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
	InstanceManager->StopCoroutines(Instance);
}

bool ULuaComponent::GetInitialized() const
{
	return Instance != nullptr;
}

bool ULuaComponent::GetLoaded() const
{
	return GetInitialized() && Instance->GetInitialized();
}

