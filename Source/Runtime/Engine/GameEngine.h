#pragma once

#include "Engine.h"
#include "Render/SkyboxRenderer.h"

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

	TUniquePtr<FSkyboxRenderer> SkyboxRenderer;
	FWorldContext* GameContext = nullptr;
};