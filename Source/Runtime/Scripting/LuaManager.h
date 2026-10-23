#pragma once

#include "Scripting/Lua.h"

class FLuaManager
{
private:

	bool bJITEnabled = true;

public:
	// 새로운 Lua 스크립트를 불러와서 FLua를 반환합니다.
	FLua LoadNewScript();

	void SetJITEnable(bool Value);
	bool GetJITEnable();
};