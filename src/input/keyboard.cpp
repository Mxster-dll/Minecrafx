#include "keyboard.h"

KeyboardInput::KeyboardInput(HWND hwnd)
    : m_hwnd(hwnd)
{}

void KeyboardInput::update()
{
    // 备份上一帧状态
    for (int i = 0; i < KEY_COUNT; ++i)
        m_prevState[i] = m_currState[i];

    // 窗口失焦 → 全部清零，防止后台误触
    if (GetForegroundWindow() != m_hwnd)
    {
        for (int i = 0; i < KEY_COUNT; ++i)
            m_currState[i] = false;
        return;
    }

    // 读取当前物理按键状态
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

bool KeyboardInput::isKeyReleased(Key key) const
{
    int code = static_cast<int>(key);
    if (code < 0 || code >= KEY_COUNT) return false;

    return !m_currState[code] && m_prevState[code];
}
