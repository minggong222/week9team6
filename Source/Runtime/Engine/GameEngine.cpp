#include "EnginePCH.h"
#include "GameEngine.h"
#include "Core/EngineTimer.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Camera/CameraActor.h"
#include "Components/TextRenderComponent.h"
#include "UObject/UObjectIterator.h"

#include "Launch/LaunchEngineLoop.h"
#include "Input/InputSystem.h"
#include "Input/KeyCode.h"
#include "Input/MouseButton.h"
#include "Render/SceneRenderer.h"
#include "Render/SceneRenderOptions.h"
#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderCommand.h"
#include "Render/Swapchain.h"
#include "Serialization/JsonArchive.h"
FEngineConfig UGameEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori";
	Desc.Width = 0;
	Desc.Height = 0;
	Desc.bBorderless = true;
	Desc.SyncInterval = 0;   // 0 = VSync 끔 (FPS 측정용)
	Desc.bCreateDepthBuffer = true;   
	Desc.bExitOnEscape = true;        
	return Desc;
}

bool UGameEngine::Init()
{
	if (!Super::Init()) return false;

	UWorld* GameWorld = UWorld::CreateWorld(EWorldType::Game, true);
	if (!GameWorld)
		return false;

	FWorldContext& Context = CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(GameWorld);

	GameContext = &Context;

	wchar_t ExePath[MAX_PATH]{};
	const DWORD PathLength = GetModuleFileNameW(nullptr, ExePath, MAX_PATH);

	if (PathLength == 0 || PathLength >= MAX_PATH)
	{
		HTR_LOG(Error, "Failed to get executable path.");
		return false;
	}

	const std::filesystem::path ExeDirectory = std::filesystem::path(std::wstring(ExePath, PathLength)).parent_path();

	const std::filesystem::path SceneFile = ExeDirectory / L"Scene" / L"Startup.scene";

	const FString StartupScenePath = SceneFile.string();

	if (!std::filesystem::exists(SceneFile))
	{
		HTR_LOG(Error, "Startup scene not found: {}", StartupScenePath);
		return false;
	}
	if (!FJsonArchive::LoadWorld(GameWorld, StartupScenePath))
	{
		HTR_LOG(Error, "Failed to load startup scene: {}", StartupScenePath);
		return false;
	}

	UGameInstance* GameInstance = NewObject<UGameInstance>(this);
	if (!GameInstance)
		return false;

	GameInstance->SetWorldContext(GameContext);
	GameInstance->Init();

	GameContext->GameViewport = MakeShared<FGameViewportClient>();
	GameContext->GameViewport->Init(*GameContext, this);

	GameContext->World()->BeginPlay();
	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	SkyboxRenderer = MakeUnique<FSkyboxRenderer>();

	if (!SkyboxRenderer->Init("Assets/SkySphere/Sky.hdr"))
	{
		HTR_LOG(Warning, "Failed to initialize game skybox");
		SkyboxRenderer.reset();
	}

	return true;
}

void UGameEngine::Tick(float DeltaTime)
{
	if (!GameContext || !GameContext->World() || !GameContext->GameViewport)
	{
		return;
	}
	TickGameInput(DeltaTime);
	GameContext->World()->Tick(DeltaTime, EngineTimer::GetRealDeltaTime());

	RenderGameFrame();
}

void UGameEngine::PreExit()
{
	Super::PreExit();
	SkyboxRenderer.reset();
	TextRenderer.reset();
	if (!GameContext)
		return;
	UWorld* GameWorld = GameContext->World();
	if (!GameWorld)
	{
		DestroyWorldContext(nullptr);
		GameContext = nullptr;
		return;
	}

	// 레벨 밖의 카메라도 삭제 전에 컴포넌트 등록을 해제한다.
	if (ACameraActor* Camera = GameWorld->GetMainCamera())
		Camera->UnregisterAllComponents();

	GameWorld->ClearWorld();

	ACameraActor* MainCamera = GameWorld->GetMainCamera();
	GameWorld->SetMainCamera(nullptr);

	DestroyWorldContext(GameWorld);
	GameContext = nullptr;

	for (ULevel* Level : GameWorld->GetLevels())
		delete Level;

	if (MainCamera)
		delete MainCamera;

	delete GameWorld;
}

void UGameEngine::TickGameInput(float DeltaTime)
{
	if (!GameContext || !GameContext->GameViewport)
		return;

	FGameViewportClient& Viewport = *GameContext->GameViewport;

	constexpr EKeyCode Keys[] =
	{
		EKeyCode::W,
		EKeyCode::A,
		EKeyCode::S,
		EKeyCode::D,
		EKeyCode::Space,
		EKeyCode::Shift
	};

	for (EKeyCode Key : Keys)
	{
		const int32 KeyCode = static_cast<int32>(Key);

		if (FInputSystem::IsKeyPressed(Key))
			Viewport.InputKey(nullptr, KeyCode, true);

		if (FInputSystem::IsKeyReleased(Key))
			Viewport.InputKey(nullptr, KeyCode, false);
	}

	constexpr EMouseButton MouseButtons[] =
	{
		EMouseButton::Left,
		EMouseButton::Right
	};

	for (EMouseButton Button : MouseButtons)
	{
		const int32 KeyCode = Button == EMouseButton::Left ? static_cast<int32>(EKeyCode::LButton) : static_cast<int32>(EKeyCode::RButton);

		if (FInputSystem::IsMousePressed(Button))
			Viewport.InputKey(nullptr, KeyCode, true);

		if (FInputSystem::IsMouseReleased(Button))
			Viewport.InputKey(nullptr, KeyCode, false);
	}

	const float MouseX = static_cast<float>(FInputSystem::GetMouseDeltaX());
	const float MouseY = static_cast<float>(FInputSystem::GetMouseDeltaY());

	if (MouseX != 0.0f)
		Viewport.InputAxis(nullptr, 0, EGameInputAxis::MouseX, MouseX, DeltaTime);

	if (MouseY != 0.0f)
		Viewport.InputAxis(nullptr, 0, EGameInputAxis::MouseY, MouseY, DeltaTime);

	const float Wheel = static_cast<float>(FInputSystem::GetWheelDelta());

	if (Wheel != 0.0f)
		Viewport.InputAxis(nullptr, 0, EGameInputAxis::MouseWheel, Wheel, DeltaTime);
}

void UGameEngine::RenderGameFrame()
{
	if (!GameContext || !GameContext->World() || !GameContext->GameViewport)
	{
		return;
	}

	FRenderer* Renderer = GetEngineLoop().GetRenderer();

	const uint32 Width = GetEngineLoop().GetViewportWidth();
	const uint32 Height = GetEngineLoop().GetViewportHeight();

	if (!Renderer || Width == 0 || Height == 0)
		return;

	FGameViewportClient& GameViewport = *GameContext->GameViewport;
	UWorld* World = GameContext->World();

	const FRect ViewRect{
		0.0f,0.0f,static_cast<float>(Width),static_cast<float>(Height)
	};

	FSceneView SceneView = GameViewport.CalcSceneView(ViewRect);

	FSceneRenderOptions Options;
	Options.ColorGradingGamma = SceneView.ColorGradingGamma;
	Options.bDrawPrimitives = true;
	Options.bCacheShadows = true;
	Options.PcfMode = EPcfMode::Sample8;
	Options.RenderingPath = ERenderingPath::Deferred;
	Options.LightingModel = ELightingModel::Gouraud;
	Options.LightCullingMode = ELightCullingMode::Tiled;

	Options.DirectionalShadowMapSize = 1024;
	Options.PointShadowMapSize = 512;
	Options.SpotShadowMapSize = 512;

	FSceneRenderer SceneRenderer(World, SceneView, GameViewport.GetRenderData());

	SceneRenderer.InitViews(Renderer, Options);

	FSceneViewTargets& Targets = GameViewport.GetViewTargets();
	FScenePassCallbacks Callbacks{};
	Callbacks.SceneText = [this, World, &SceneView] { RenderSceneText(World, SceneView); };

	FRenderGraphBuilder Graph;

	if (!SceneRenderer.AddViewportPasses(Graph, Renderer, Targets, Options, SkyboxRenderer.get(), Callbacks))
	{
		return;
	}

	if (!Graph.Execute() || !SceneRenderer.DidCorePassSucceed() || !SceneRenderer.DidFXAAPassSucceed())
		return;

	RenderCommand::EndRenderPass();

	FTexture2D* SceneColor = GameViewport.GetColorTarget();
	FTexture2D* Backbuffer = GetEngineLoop().GetSwapchain()->GetBackbuffer();

	if (!SceneColor || !Backbuffer)
		return;

	if (SceneColor->GetWidth() != Backbuffer->GetWidth() || SceneColor->GetHeight() != Backbuffer->GetHeight() || SceneColor->GetFormat() != Backbuffer->GetFormat())
		return;

	RenderCommand::GetContext()->CopyResource(Backbuffer->GetRawPtr(), SceneColor->GetRawPtr());
}

void UGameEngine::RenderSceneText(UWorld* World, const FSceneView& SceneView)
{
	if (!TextRenderer) return;
	for (TObjectIterator<UTextRenderComponent> Text; Text; ++Text)
	{
		if (!Text || !Text->IsRegistered() || !Text->GetFont() || !Text->IsShown(SceneView.bGameView) ||
			!Text->GetOwner() || Text->GetOwner()->GetWorld() != World)
			continue;
		TextRenderer->OnRender(Text->GetText(), Text->GetWorldMatrix(), Text->GetTextSize(),
			*Text->GetFont(), SceneView.ViewProjectionMatrix);
	}
}

