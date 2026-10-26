#include "EnginePCH.h"
#include "LuaComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Scripting/LuaInstance.h"

namespace
{
	// 생명주기 함수는 생략할 수 있으며, 실행 중 발생한 오류만 로그로 남깁니다.
	template<typename... Args>
	void InvokeLifecycle(FLuaInstance& Instance, FStringView FunctionName, Args... args)
	{
		auto Result = Instance.Invoke(FunctionName, args...);
		if (Result && !Result->valid())
		{
			sol::error Error = *Result;
			HTR_LOG(Error, "Lua 함수 실행 실패: {}", Error.what());
		}
	}
}

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
	if (!Instance || ScriptPath.empty() || !GEngine)
	{
		return false;
	}

	const FLuaScriptSource* Script = GEngine->GetLuaRegistry().FindOrLoad(ScriptPath);
	if (!Script || !Instance->Load(ScriptPath, Script->Source))
	{
		Instance->Unload();
		return false;
	}

	return true;
}

bool ULuaComponent::ReloadLua()
{
	const UWorld* World = GetWorld();
	if (!GEngine || !World || ScriptPath.empty())
	{
		return false;
	}

	if (World->GetWorldType() == EWorldType::Editor)
	{
		return GEngine->GetLuaRegistry().Reload(ScriptPath);
	}

	return GetLoaded() && InstanceManager && InstanceManager->RequestHotReload(Instance);
}

sol::variadic_results ULuaComponent::Invoke(const FString& FunctionName, sol::variadic_args Args)
{
	if (!GetLoaded())
	{
		throw sol::error("Invoke: script is not loaded");
	}

	auto Result = Instance->Invoke(FunctionName, Args);
	if (!Result)
	{
		throw sol::error("Invoke: function not found: " + FunctionName);
	}

	if (!Result->valid())
	{
		sol::error Error = *Result;
		throw Error;
	}

	// 결과가 Lua 스택에서 제거되기 전에 nil을 포함한 모든 반환값을 확보합니다.
	return sol::variadic_results(std::move(*Result));
}

bool ULuaComponent::SetScriptPath(const FString& InPath)
{
	// 연결 시 소스만 확보합니다. 게임 코드 실행은 플레이 초기화까지 미룹니다.
	if (!InPath.empty() && GEngine && !GEngine->GetLuaRegistry().FindOrLoad(InPath))
	{
		return false;
	}

	ScriptPath = InPath;
	return true;
}

void ULuaComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	// 씬 로드는 컴포넌트를 먼저 등록한 뒤 경로를 읽으므로, 역직렬화 후 소스를 확보합니다.
	// PIE 복제는 이미 채택한 소스를 사용하며 파일을 다시 읽지 않습니다.
	FArchive& Archive = Record.GetUnderlyingArchive();
	if (GEngine && Archive.IsLoading() && !Archive.HasAnyPortFlags(EPropertyPortFlags::PPF_Duplicate)
		&& !ScriptPath.empty())
	{
		GEngine->GetLuaRegistry().FindOrLoad(ScriptPath);
	}
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
		InvokeLifecycle(*Instance, "InitializeComponent", this);
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

	InvokeLifecycle(*Instance, "BeginPlay", this);
}

void ULuaComponent::TickComponent(float DeltaSeconds)
{
	Super::TickComponent(DeltaSeconds);

	if (!GetLoaded())
	{
		return;
	}

	InvokeLifecycle(*Instance, "TickComponent", this, DeltaSeconds);
}

void ULuaComponent::EndPlay()
{
	Super::EndPlay();

	if (!GetLoaded())
	{
		return;
	}

	InvokeLifecycle(*Instance, "EndPlay", this);
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

