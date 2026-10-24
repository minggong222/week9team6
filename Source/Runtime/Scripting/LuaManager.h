#pragma once

#include "Scripting/LuaInstance.h"
#include "Containers/Array.h"
#include "Core/Types.h"

class ULuaComponent;

class FLuaManager
{
public:
    FLuaManager() = default;
    ~FLuaManager() = default;
    FLuaManager(const FLuaManager&) = delete;
    FLuaManager& operator=(const FLuaManager&) = delete;

    void Initialize();
    FLuaInstance* CreateInstance(
        FStringView Path,
        UObject* Owner
    );

    bool SetJITEnable(bool Value);
    bool GetJITEnable() const { return bJITEnabled; }

    bool ReloadAll();
    bool Reload(int32 Handle);
    bool Remove(int32 Handle);
    bool Remove(FLuaInstance* Instance);

private:

    bool CheckRange(int32 Index) const;

    sol::state State;
    bool bJITEnabled = true;
    
    TArray<TUniquePtr<FLuaInstance>> Instances;
};
