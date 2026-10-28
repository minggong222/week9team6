#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

class UWorld;
class UEngine;
class FLuaManager;
struct FWorldContext;

class UGameInstance : public UObject
{
	DECLARE_CLASS(UGameInstance, UObject)

public:
	UGameInstance();
	virtual ~UGameInstance() override;

	virtual void Init();
	virtual void Shutdown();
	UWorld* GetWorld() const;
	FWorldContext* GetWorldContext() const { return WorldContext; }
	
	void SetWorldContext(FWorldContext* Context);
	virtual bool InitializeForPlayInEditor(int32 PIEInstance);
	virtual bool StartPlayInEditorGameInstance();
	
	UEngine* GetEngine() const;
	FLuaManager* GetLuaManager();
private:

	// Lua 구현 헤더가 엔진 사용자에게 전파되지 않도록 소유권만 보관합니다.
	TUniquePtr<FLuaManager> LuaManager;
	FWorldContext* WorldContext = nullptr;

};
