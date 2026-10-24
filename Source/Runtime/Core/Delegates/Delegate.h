#pragma once
#include <functional>
#include <vector>

template<typename... Args>
class TDelegate
{
public:
	using HandlerType = std::function<void(Args...)>;

	// 일반 함수나 람다 등록
	// TODO: 구독 식별 위해 Delegate Handle 반환하도록 추가
	void Add(const HandlerType& handler)
	{
		Handlers.push_back(handler);
	}

	// 클래스 멤버 함수 바인딩
	// TODO: 구독 식별 위해 Delegate Handle 반환하도록 추가
	template<typename T>
	void AddDynamic(T* Instance, void (T::* Func)(Args...))
	{
		Handlers.push_back([Instance, Func](Args... args) {(Instance->*Func)(args...);});
	}

	// TODO: Delegate 구독 해제 및 생명주기 안전성 보강
	void Broadcast(Args... args)
	{
		for (auto Handler : Handlers)
		{
			Handler(args...);
		}
	}

	//  TODO: Clear(), Remove(Handle), RemoveAll(Object) 함수 추가
	void Clear()
	{
		Handlers.clear();
	}


private:
	std::vector<HandlerType> Handlers;
};

#define DECLARE_DELEGATE(Name, ...) TDelegate<__VA_ARGS__> Name