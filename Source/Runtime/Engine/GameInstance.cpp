#include "EnginePCH.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"

void UGameInstance::Init()
{
	LuaManager.Initialize(GetEngine()->GetLuaRegistry());
}

void UGameInstance::Shutdown()
{
	if (!WorldContext) return;
	if (UWorld* World = GetWorld()) World->SetGameInstance(nullptr);
	if (WorldContext->GameViewport)
	{
		WorldContext->GameViewport->Reset();
		WorldContext->GameViewport.reset();
	}
	WorldContext->OwningGameInstance = nullptr;
	WorldContext = nullptr;
}

UWorld* UGameInstance::GetWorld() const
{
	return WorldContext ? WorldContext->World() : nullptr;
}

void UGameInstance::SetWorldContext(FWorldContext* Context)
{
	if (WorldContext && WorldContext->OwningGameInstance == this)
	{
		if (UWorld* World = GetWorld()) World->SetGameInstance(nullptr);
		WorldContext->OwningGameInstance = nullptr;
	}
	WorldContext = Context;
	if (Context)
	{
		Context->OwningGameInstance = this;
		if (UWorld* World = GetWorld()) World->SetGameInstance(this);
	}
}

bool UGameInstance::InitializeForPlayInEditor(int32 PIEInstance)
{
	UEngine* Engine = GetEngine();
	if (!Engine) return false;
	FWorldContext* Context = Engine->GetWorldContextFromPIEInstance(PIEInstance);
	if (!Context || !Engine->GetEditorWorld()) return false;
	SetWorldContext(Context);
	if (!Engine->CreatePIEWorldByDuplication(*Context, Engine->GetEditorWorld())) return false;
	Init();
	return true;
}

bool UGameInstance::StartPlayInEditorGameInstance()
{
	UWorld* World = GetWorld();
	if (!World || !WorldContext) return false;
	WorldContext->GameViewport = MakeShared<FGameViewportClient>();
	WorldContext->GameViewport->Init(*WorldContext, GetEngine());
	World->BeginPlay();
	return true;
}

UEngine* UGameInstance::GetEngine() const
{
	return GEngine;
}

FLuaManager* UGameInstance::GetLuaManager()
{
	return &LuaManager;
}
