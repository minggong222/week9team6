#pragma once

#include "Containers/Array.h"
#include "Core/Delegates/Delegate.h"
#include "Core/EngineString.h"
#include "Core/EngineLog.h"
#include "sol/sol.hpp"

// 템플릿은 Lua에 직접 노출할 수 없으므로 사용하는 타입별로 등록합니다.
template<typename... Args>
void LuaRegisterDelegate(sol::state& State, const char* Name)
{
    using DelegateType = TDelegate<Args...>;

    State.new_usertype<DelegateType>(Name, sol::constructors<DelegateType()>(),
        "Add", [](DelegateType& Delegate, sol::protected_function Function)
        {
            if (!Function.valid())
            {
                throw sol::error("Delegate.Add requires a function");
            }

            // 함수 참조는 등록 해제까지 유지되므로 스크립트 종료 시 Remove해야 합니다.
            return Delegate.Add([Function = std::move(Function)](Args... Arguments)
            {
                sol::protected_function_result Result = Function(Arguments...);
                if (!Result.valid())
                {
                    sol::error Error = Result;
                    HTR_LOG(Error, "Lua Delegate 콜백 실행 실패: {}", Error.what());
                }
            });
        },
        "Remove", &DelegateType::Remove,
        "Clear", &DelegateType::Clear,
        "Broadcast", &DelegateType::Broadcast);
}

template<typename T>
void LuaRegisterArray(sol::state& State, const char* Name)
{
    using ArrayType = TArray<T>;

    State.new_usertype<ArrayType>(Name, sol::constructors<ArrayType()>(),
        "Num", &ArrayType::Num,
        "IsEmpty", &ArrayType::IsEmpty,
        "IsValidIndex", &ArrayType::IsValidIndex,
        "Add", [](ArrayType& Array, const T& Value) { return Array.Add(Value); },
        "Get", [](const ArrayType& Array, int32 Index) -> T
        {
            // C++의 TArray와 동일하게 0부터 시작하며, 잘못된 접근은 Lua 오류로 처리합니다.
            if (!Array.IsValidIndex(Index))
            {
                throw sol::error("TArray index out of range");
            }

            return Array[Index];
        },
        "Set", [](ArrayType& Array, int32 Index, const T& Value)
        {
            if (!Array.IsValidIndex(Index))
            {
                throw sol::error("TArray index out of range");
            }

            Array[Index] = Value;
        },
        "RemoveAt", [](ArrayType& Array, int32 Index)
        {
            if (!Array.IsValidIndex(Index))
            {
                throw sol::error("TArray index out of range");
            }

            Array.RemoveAt(Index);
        },
        "Clear", [](ArrayType& Array) { Array.Empty(); });
}
