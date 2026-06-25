#include "keyboard.h"

KeyboardInput::KeyboardInput(HWND hwnd)
    : m_hwnd(hwnd)
{}

void KeyboardInput::update()
{

    for (int i = 0; i < KEY_COUNT; ++i)
        m_prevState[i] = m_currState[i];

    if (GetForegroundWindow() != m_hwnd)
    {
        for (int i = 0; i < KEY_COUNT; ++i)
            m_currState[i] = false;
        return;
    }

    for (int i = 0; i < KEY_COUNT; ++i)
        m_currState[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
}

bool KeyboardInput::isKeyDown(Key key) const
{
    int code = static_cast<int>(key);
    if (code < 0 || code >= KEY_COUNT) return false;

    return m_currState[code];
}

bool KeyboardInput::isPressed(Key key) const
{
    int code = static_cast<int>(key);
    if (code < 0 || code >= KEY_COUNT) return false;

    return m_currState[code] && !m_prevState[code];
}