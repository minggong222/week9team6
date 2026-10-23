#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>
#include <algorithm>

template<typename... Args>
class TDelegate;

class FDelegateHandle
{
public:
	FDelegateHandle() = default;
	bool IsValid() const noexcept
	{
		return ID != 0;
	}

	void Reset() noexcept
	{
		ID = 0;
	}

	bool operator==(const FDelegateHandle& Other) const noexcept
	{
		return ID == Other.ID;
	}
private:
	explicit FDelegateHandle(uint64 InID) : ID(InID) {}
	static FDelegateHandle Generate()
	{
		static std::atomic<uint64> NextID{ 1 };
		return FDelegateHandle{ NextID.fetch_add(1, std::memory_order_relaxed) };
	}
	uint64 ID = 0;
	template<typename... Args>
	friend class TDelegate;
};

template<typename... Args>
class TDelegate
{
public:
	using HandlerType = std::function<void(Args...)>;

	// 일반 함수나 람다 등록
	FDelegateHandle Add(const HandlerType& handler)
	{
		FDelegateHandle Handle = FDelegateHandle::Generate();

		Handlers.push_back({Handle, handler });

		return Handle;
	}

	// 클래스 멤버 함수 바인딩
	template<typename T>
	FDelegateHandle AddDynamic(T* Instance, void (T::* Func)(Args...))
	{
		FDelegateHandle Handle = FDelegateHandle::Generate();

		Handlers.push_back({ Handle, [Instance, Func](Args... args) {(Instance->*Func)(args...);}, Instance });

		return Handle;
	}

	void Broadcast(Args... args)
	{
		for (const FHandlerEntry& Entry : Handlers)
		{
			Entry.Callback(args...);
		}
	}
	
	// 해당 Handle 제거
	bool Remove(const FDelegateHandle& Handle)
	{
		if (!Handle.IsValid())
		{
			return false;
		}

		auto It = std::remove_if(Handlers.begin(), Handlers.end(), [&Handle](const FHandlerEntry& Entry) {return Entry.Handle == Handle;});
		if (It == Handlers.end())
		{
			return false;
		}
		Handlers.erase(It, Handlers.end());
		return true;
	}

	// 모든 등록 제거
	void Clear()
	{
		Handlers.clear();
	}

	// 해당 객체 등록 모두 제거
	size_t RemoveAll(const void* Instance)
	{
		if (Instance == nullptr)
		{
			return 0;
		}
		const size_t OldSize = Handlers.size();
		
		auto It = std::remove_if(Handlers.begin(), Handlers.end(), [Instance](const FHandlerEntry& Entry) {return Entry.Owner == Instance;});

		Handlers.erase(It, Handlers.end());

		// 제거한 등록 개수 반환
		return OldSize - Handlers.size();
	}
private:
	struct FHandlerEntry
	{
		FDelegateHandle Handle;
		HandlerType Callback;
		const void* Owner = nullptr;
	};
	std::vector<FHandlerEntry> Handlers;
};

#define DECLARE_DELEGATE(Name, ...) TDelegate<__VA_ARGS__> Name