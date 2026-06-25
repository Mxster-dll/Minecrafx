#pragma once

#include <windows.h>

enum Key : int
{
    A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G',
    H = 'H', I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N',
    O = 'O', P = 'P', Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U',
    V = 'V', W = 'W', X = 'X', Y = 'Y', Z = 'Z',

    Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4',
    Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9',

    F1 = VK_F1, F2 = VK_F2, F3 = VK_F3, F4 = VK_F4,
    F5 = VK_F5, F6 = VK_F6, F7 = VK_F7, F8 = VK_F8,
    F9 = VK_F9, F10 = VK_F10, F11 = VK_F11, F12 = VK_F12,

    Left = VK_LEFT, Right = VK_RIGHT, Up = VK_UP, Down = VK_DOWN,

    LShift = VK_LSHIFT, RShift = VK_RSHIFT,
    LCtrl = VK_LCONTROL, RCtrl = VK_RCONTROL,
    LAlt = VK_LMENU, RAlt = VK_RMENU,

    Space = VK_SPACE,
    Enter = VK_RETURN,
    Esc = VK_ESCAPE,
    Tab = VK_TAB,
    Backspace = VK_BACK,
    Delete = VK_DELETE,
    Insert = VK_INSERT,
    Home = VK_HOME,
    End = VK_END,
    PageUp = VK_PRIOR,
    PageDown = VK_NEXT,
    CapsLock = VK_CAPITAL,
    PrintScreen = VK_SNAPSHOT,
};

class KeyboardInput
{
public:

    explicit KeyboardInput(HWND hwnd);

    void update();

    bool isKeyDown(Key key) const;

    bool isPressed(Key key) const;

private:
    static constexpr int KEY_COUNT = 256;
    HWND m_hwnd;
    bool m_currState[KEY_COUNT] = {};
    bool m_prevState[KEY_COUNT] = {};
};