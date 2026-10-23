#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Containers/StringView.h"

#include "sol/sol.hpp"

class FLua
{
private: 

	// 스크립트의 경로를 가리킵니다.
	FString Path;

	sol::state State;

	void Initalize();

	// C++ 함수, 클래스 API를 Lua에 붙이기
	void RegisterAPI();

public:


	bool LoadFromFile(FStringView InPath);


};