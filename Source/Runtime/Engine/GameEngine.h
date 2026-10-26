#pragma once

#include "Engine.h"
#include "Render/SkyboxRenderer.h"
#include "Text/TextRenderer.h"

class UGameEngine : public UEngine
{
	DECLARE_CLASS(UGameEngine, UEngine)
public:
	FEngineConfig GetConfig() const override;

	virtual bool Init() override;
	virtual void Tick(float DeltaTime)override;

	void PreExit() override;
private:
	void TickGameInput(float DeltaTime);
	void RenderGameFrame();
	void RenderSceneText(UWorld* World, const FSceneView& SceneView);

	TUniquePtr<FSkyboxRenderer> SkyboxRenderer;
	TUniquePtr<FTextRenderer> TextRenderer;
	FWorldContext* GameContext = nullptr;
};
