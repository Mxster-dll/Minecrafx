#include "input_handler.h"

static const wchar_t *PROP_NAME = L"InputHandler_This";

InputHandler::InputHandler(HWND hwnd)
    : m_hwnd(hwnd)
    , m_keyboard(hwnd)
    , m_mouse(hwnd)
    , m_wheelDelta(0)
    , m_oldWndProc(nullptr)
{
    for (int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
    {
        m_mouseCurr[i] = false;
        m_mousePrev[i] = false;
        m_clickConsumed[i] = false;
        m_releaseConsumed[i] = false;
    }

    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = m_hwnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    SetProp(m_hwnd, PROP_NAME, reinterpret_cast<HANDLE>(this));
    m_oldWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(m_hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(wndProcHook)));
}

InputHandler::~InputHandler()
{

    if (m_oldWndProc)
    {
        SetWindowLongPtr(m_hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(m_oldWndProc));
    }
    RemoveProp(m_hwnd, PROP_NAME);

    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = RIDEV_REMOVE;
    rid.hwndTarget = nullptr;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

LRESULT CALLBACK InputHandler::wndProcHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    InputHandler *self = reinterpret_cast<InputHandler *>(
        GetProp(hwnd, PROP_NAME));

    if (msg == WM_INPUT && self)
    {
        UINT size = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT,
            nullptr, &size, sizeof(RAWINPUTHEADER));
        if (size > 0 && size <= sizeof(RAWINPUT))
        {
            RAWINPUT raw;
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT,
                &raw, &size, sizeof(RAWINPUTHEADER)) == size)
            {
                if (raw.header.dwType == RIM_TYPEMOUSE)
                {

                    USHORT flags = raw.data.mouse.usButtonFlags;
                    if (flags & RI_MOUSE_WHEEL)
                    {

                        self->m_wheelDelta += static_cast<short>(
                            raw.data.mouse.usButtonData);
                    }
                }
            }
        }
    }

    else if (msg == WM_MOUSEWHEEL && self)
    {
        self->m_wheelDelta += GET_WHEEL_DELTA_WPARAM(wp);
    }

    if (self && self->m_oldWndProc)
        return CallWindowProc(self->m_oldWndProc, hwnd, msg, wp, lp);

    return DefWindowProc(hwnd, msg, wp, lp);
}

void InputHandler::update()
{

    m_keyboard.update();

    for (int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
    {
        m_mousePrev[i] = m_mouseCurr[i];

        if (!m_mouseCurr[i])
        {
            m_clickConsumed[i] = false;
            m_releaseConsumed[i] = false;
        }
    }

    if (GetForegroundWindow() != m_hwnd)
    {
        for (int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
            m_mouseCurr[i] = false;
        return;
    }

    m_mouseCurr[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    m_mouseCurr[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    m_mouseCurr[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
}

bool InputHandler::isKeyDown(Key key) const
{
    return m_keyboard.isKeyDown(key);
}

bool InputHandler::isPressed(Key key) const
{
    return m_keyboard.isPressed(key);
}

std::pair<int, int> InputHandler::getMouseDelta()
{
    return m_mouse.getDelta();
}

bool InputHandler::getMouseClick(int button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;

    bool clicked = m_mouseCurr[button] && !m_mousePrev[button] && !m_clickConsumed[button];

    if (clicked)
        m_clickConsumed[button] = true;

    return clicked;
}

bool InputHandler::getMouseRelease(int button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;

    bool released = m_mousePrev[button] && !m_mouseCurr[button] && !m_releaseConsumed[button];

    if (released)
        m_releaseConsumed[button] = true;

    return released;
}

int InputHandler::getMouseWheel()
{
    int delta = m_wheelDelta;
    m_wheelDelta = 0;
    return delta;
}

POINT InputHandler::getMouseScreenPos() const
{
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(m_hwnd, &pt);
    return pt;
}

bool InputHandler::isMouseButtonDown(int button) const
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;
    return m_mouseCurr[button];
}

void InputHandler::showMouseCursor(bool visible)
{
    if (visible)
        m_mouse.showCursor();
    else
        m_mouse.hideCursor();
}