#pragma once

#include <windows.h>
#include <utility>
#include "keyboard.h"
#include "mouse.h"

class InputHandler
{
public:

    explicit InputHandler(HWND hwnd);

    ~InputHandler();

    void update();

    bool isKeyDown(Key key) const;

    bool isPressed(Key key) const;

    std::pair<int, int> getMouseDelta();

    bool getMouseClick(int button);

    POINT getMouseScreenPos() const;

    bool getMouseRelease(int button);

    bool isMouseButtonDown(int button) const;

    void showMouseCursor(bool visible);

    void requestQuit() { m_quitRequested = true; }

    bool isQuitRequested() const { return m_quitRequested; }

    int getMouseWheel();

private:
    HWND m_hwnd;
    KeyboardInput m_keyboard;
    MouseInput m_mouse;

    static constexpr int MOUSE_BUTTON_COUNT = 3;
    bool m_mouseCurr[MOUSE_BUTTON_COUNT];
    bool m_mousePrev[MOUSE_BUTTON_COUNT];
    bool m_clickConsumed[MOUSE_BUTTON_COUNT];
    bool m_releaseConsumed[MOUSE_BUTTON_COUNT];

    int m_wheelDelta;

    bool m_quitRequested = false;

    WNDPROC m_oldWndProc;
    static LRESULT CALLBACK wndProcHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};