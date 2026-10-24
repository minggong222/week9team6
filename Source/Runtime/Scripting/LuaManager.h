#pragma once

#include "Scripting/LuaInstance.h"

#include "sol/sol.hpp"

class FLuaManager
{
private:

	sol::state State;
	bool bJITEnabled = true;

public:

	void Initialize();

	// 새로운 Lua 스크립트를 불러와서 FLua를 반환합니다.
	FLuaInstance Load(FStringView Path);

	bool SetJITEnable(bool Value);
	bool GetJITEnable() { return bJITEnabled; }
};