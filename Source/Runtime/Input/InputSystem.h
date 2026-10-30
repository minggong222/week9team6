#pragma once

#include "KeyCode.h"
#include "MouseButton.h"
#include "Core/Types.h"
#include <optional>

enum class EGameInputAxis : uint8 { MouseX, MouseY, MouseWheel };

class FInputSystem
{
public:
    // 월드 Tick 동안만 게임 입력을 막고, 에디터의 카메라 입력은 유지합니다.
    class FScopedInputBlock
    {
    public:
        explicit FScopedInputBlock(bool Block, std::optional<EKeyCode> AllowedKey = std::nullopt)
            : bPrevious(bInputBlocked), PreviousAllowedKey(UnblockedKey)
        {
            if (Block)
            {
                bInputBlocked = true;
                UnblockedKey = AllowedKey;
            }
        }
        ~FScopedInputBlock() { bInputBlocked = bPrevious; UnblockedKey = PreviousAllowedKey; }
        FScopedInputBlock(const FScopedInputBlock&) = delete;
        FScopedInputBlock& operator=(const FScopedInputBlock&) = delete;
    private:
        bool bPrevious;
        std::optional<EKeyCode> PreviousAllowedKey;
    };

    static bool IsKeyPressed(EKeyCode KeyCode);
    static bool IsKeyReleased(EKeyCode KeyCode);
    static bool IsKeyDown(EKeyCode KeyCode);

    static bool IsMouseDown(EMouseButton MouseButton);
    static bool IsMousePressed(EMouseButton MouseButton);
    static bool IsMouseReleased(EMouseButton MouseButton);

    static void UpdateInputStates();
    static void ClearAllStates();
    static void SyncMousePosition(int32 x, int32 y);
    static void OnKeyUp(int VirtualKey);
    static void OnMouseUp(EMouseButton MouseButton);
    static void OnKeyDown(int VirtualKey);
    static void OnMouseDown(EMouseButton MouseButton);
    static void OnMouseMove(int32 x, int32 y);
    static void OnMouseWheelDelta(int32 Wheel);

    static inline int32 GetMouseX() { return MouseX; }
    static inline int32 GetMouseY() { return MouseY; }

    static inline int32 GetMouseDeltaX() { return DeltaX; }
    static inline int32 GetMouseDeltaY() { return DeltaY; }

    // TODO: WheelDelta를 소모하지 않는 경로가 있어야할 듯
    static inline int32 GetWheelDelta() 
    { 
        int32 Delta = WheelDelta;
        WheelDelta = 0;
        return Delta;
    }

private:
    FInputSystem() = delete;
    static inline bool bInputBlocked = false;
    static inline std::optional<EKeyCode> UnblockedKey;

    static inline bool bMouseStates[5] = {};
    static inline bool bPrevMouseStates[5] = {};

    static inline bool bKeyStates[256] = {};
    static inline bool bPrevKeyStates[256] = {};

    static inline int32 MouseX = 0, MouseY = 0;
    static inline int32 PrevMouseX = 0, PrevMouseY = 0;
    static inline int32 DeltaX = 0, DeltaY = 0;

    static inline int32 WheelDelta = 0;
};
