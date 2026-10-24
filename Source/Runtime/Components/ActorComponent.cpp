#include "EnginePCH.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

UActorComponent::~UActorComponent()
{
	assert(!bRegistered && "Delete Without Unregister");
	if (Owner)
	{
		Owner->RemoveOwnedComponent(this);
	}
}

void UActorComponent::SetActive(bool bNewActive)
{
	if (bNewActive) Activate();
	else Deactivate();
}

void UActorComponent::SetVisibility(bool bNewVisibility)
{
	if (bVisible == bNewVisibility) return;
	bVisible = bNewVisibility;
	MarkRenderStateDirty();
}

void UActorComponent::SetHiddenInGame(bool bNewHidden)
{
	if (bHiddenInGame == bNewHidden) return;
	bHiddenInGame = bNewHidden;
	MarkRenderStateDirty();
}

void UActorComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "bVisible" || Property.Name == "bHiddenInGame")
		MarkRenderStateDirty();
}

void UActorComponent::BeginPlayComponent()
{
	if (bIsPendingDestroy || !IsRegistered() || !WorldPrivate || !WorldPrivate->HasBegunPlay())
		return;

	if (bIsPlaying)
	{
		RegisterComponentTickFunctions(true);
		return;
	}

	AActor* MyOwner = Owner;
	const bool bWasExecuting = MyOwner ? MyOwner->bIsExecutingComponentCallback : false;

	if (MyOwner)
		MyOwner->bIsExecutingComponentCallback = true;

	bIsDispatchingBeginPlay = true;

	bIsPlaying = true;
	bHasBegunPlay = true;

	if (!bIsInitialized)
		InitializeComponent();

	RegisterComponentTickFunctions(true);

	if (bAutoActivate)
		Activate();
	else
		SetComponentTickEnabled(false);

	BeginPlay();

	bIsDispatchingBeginPlay = false;

	if (MyOwner)
		MyOwner->bIsExecutingComponentCallback = bWasExecuting;
}

void UActorComponent::EndPlayComponent()
{
	if (!bIsPlaying)
		return;

	AActor* MyOwner = Owner;
	const bool bWasExecuting = MyOwner ? MyOwner->bIsExecutingComponentCallback : false;

	if (MyOwner)
		MyOwner->bIsExecutingComponentCallback = true;

	bIsDispatchingEndPlay = true;

	bIsPlaying = false;

	SetComponentTickEnabled(false);
	Deactivate();

	EndPlay();

	bIsDispatchingEndPlay = false;

	if (MyOwner)
		MyOwner->bIsExecutingComponentCallback = bWasExecuting;
}

void UActorComponent::DispatchTickComponent(float DeltaTime)
{
	if (bIsDispatchingTick)
		return;

	AActor* MyOwner = Owner;
	const bool bWasExecuting = MyOwner ? MyOwner->bIsExecutingComponentCallback : false;

	if (MyOwner)
		MyOwner->bIsExecutingComponentCallback = true;

	bIsDispatchingTick = true;
	TickComponent(DeltaTime);
	bIsDispatchingTick = false;

	if (MyOwner)
		MyOwner->bIsExecutingComponentCallback = bWasExecuting;
}

void UActorComponent::Activate(bool bReset)
{
	if (!bReset && !ShouldActivate()) return;
	SetComponentTickEnabled(true);
	bIsActive = true;
}

void UActorComponent::Deactivate()
{
	if (!bIsActive) return;
	SetComponentTickEnabled(false);
	bIsActive = false;
}

void UActorComponent::SetAutoActivate(bool bNewAutoActivate)
{
	if (IsRegistered())
	{
		HTR_LOG(Warning, "SetAutoActivate: cannot change AutoActivate after component registration");
		return;
	}
	bAutoActivate = bNewAutoActivate;
}

void UActorComponent::SetComponentTickEnabled(bool bEnabled)
{
	if (!PrimaryComponentTick.bCanEverTick) return;
	PrimaryComponentTick.bStartWithTickEnabled = bEnabled;
	PrimaryComponentTick.SetTickFunctionEnable(bEnabled);
}

void UActorComponent::RegisterComponentTickFunctions(bool bRegister)
{
	if (!bRegister)
	{
		PrimaryComponentTick.UnRegisterTickFunction();
		return;
	}
	if (IsRegistered() && WorldPrivate && PrimaryComponentTick.bCanEverTick)
		PrimaryComponentTick.RegisterTickFunction(WorldPrivate->GetTickTaskManager());
}

void UActorComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	FArchive& Ar = Record.GetUnderlyingArchive();
	if (Ar.HasAnyPortFlags(EPropertyPortFlags::PPF_Duplicate))
	{
		UObject* OwnerObj = Owner;
		Record << SA_VALUE("Owner", OwnerObj);
		Owner = static_cast<AActor*>(OwnerObj);
	}
}

void UActorComponent::RegisterComponent()
{
	AActor* MyOwner = GetOwner();
	UWorld* MyOwnerWorld = (MyOwner ? MyOwner->GetWorld() : nullptr);
	if (MyOwnerWorld)
	{
		RegisterComponentWithWorld(MyOwnerWorld);
	}
}

void UActorComponent::RegisterComponentWithWorld(UWorld* InWorld)
{
	if (IsRegistered()) { HTR_LOG(Info, "RegisterComponentWithWorld: {} Already registered. Aborting.", GetClass()->Name); return; }
	if (!IsValid(InWorld)) { return; }

	WorldPrivate = InWorld;
	ExecuteRegisterEvents();
	if (AActor* MyOwner = GetOwner(); MyOwner && MyOwner->IsPlaying())
		BeginPlayComponent();
}

void UActorComponent::UnregisterComponent()
{
	if (!IsRegistered()) return;
	RegisterComponentTickFunctions(false);
	ExecuteUnregisterEvents();
	WorldPrivate = nullptr;
}

void UActorComponent::OnRegister()
{
	bRegistered = true;
}


void UActorComponent::OnUnregister()
{
	bRegistered = false;
}

void UActorComponent::CreateRenderState()
{
	bRenderStateCreated = true;
}

void UActorComponent::DestroyRenderState()
{
	bRenderStateCreated = false;
}

void UActorComponent::ExecuteRegisterEvents()
{
	if (!bRegistered)
	{
		OnRegister();
		assert(bRegistered && "Forgot to Call OnRegister: Super ");
	}
	if (!bRenderStateCreated && ShouldCreateRenderState())
	{
		CreateRenderState();
		assert(bRenderStateCreated && "Forgot to Call CreateRenderState: Super ");
	}
}

void UActorComponent::ExecuteUnregisterEvents()
{
	if (bRenderStateCreated)
	{
		DestroyRenderState();
	}
	if (bRegistered)
	{
		OnUnregister();
	}
}
