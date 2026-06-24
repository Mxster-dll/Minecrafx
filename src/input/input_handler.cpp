#include "input_handler.h"
#include <cmath>

// 用于 SetProp/GetProp 的属性名
static const wchar_t *PROP_NAME = L"InputHandler_This";

// ============================================================================
// InputHandler
// ============================================================================

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

    // 注册 Raw Input：直接从鼠标硬件读取滚轮数据，绕过消息队列
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;        // 通用桌面
    rid.usUsage = 0x02;        // 鼠标
    rid.dwFlags = RIDEV_INPUTSINK;  // 即使非前台也接收
    rid.hwndTarget = m_hwnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    // 子类化窗口以拦截 WM_INPUT（Raw Input 通过此消息送达）
    SetProp(m_hwnd, PROP_NAME, reinterpret_cast<HANDLE>(this));
    m_oldWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(m_hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(wndProcHook)));
}

InputHandler::~InputHandler()
{
    // 恢复原窗口过程
    if (m_oldWndProc)
    {
        SetWindowLongPtr(m_hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(m_oldWndProc));
    }
    RemoveProp(m_hwnd, PROP_NAME);

    // 注销 Raw Input
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

    // 处理 Raw Input 消息
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
                    // 检查滚轮标志
                    USHORT flags = raw.data.mouse.usButtonFlags;
                    if (flags & RI_MOUSE_WHEEL)
                    {
                        // 滚轮数据在 usButtonData 中（有符号 short）
                        self->m_wheelDelta += static_cast<short>(
                            raw.data.mouse.usButtonData);
                    }
                }
            }
        }
    }
    // 后备：也拦截 WM_MOUSEWHEEL（某些系统可能仍发送此消息）
    else if (msg == WM_MOUSEWHEEL && self)
    {
        self->m_wheelDelta += GET_WHEEL_DELTA_WPARAM(wp);
    }

    // 调用原窗口过程
    if (self && self->m_oldWndProc)
        return CallWindowProc(self->m_oldWndProc, hwnd, msg, wp, lp);

    return DefWindowProc(hwnd, msg, wp, lp);
}

void InputHandler::update()
{
    // 更新键盘
    m_keyboard.update();

    // 备份上一帧鼠标按键状态
    for (int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
    {
        m_mousePrev[i] = m_mouseCurr[i];
        // 按键释放时重置消费标记，允许下次边沿触发
        if (!m_mouseCurr[i])
        {
            m_clickConsumed[i] = false;
            m_releaseConsumed[i] = false;
        }
    }

    // 窗口失焦 → 清零
    if (GetForegroundWindow() != m_hwnd)
    {
        for (int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
            m_mouseCurr[i] = false;
        return;
    }

    // 读取鼠标按键状态
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

    // 上升沿检测：当前按下且上一帧未按下，且本周期未消费
    bool clicked = m_mouseCurr[button] && !m_mousePrev[button] && !m_clickConsumed[button];

    // 标记已消费，防止同一按下周期内重复触发
    if (clicked)
        m_clickConsumed[button] = true;

    return clicked;
}

bool InputHandler::getMouseRelease(int button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;

    // 下降沿检测：上一帧按下且当前帧未按下，且未消费
    bool released = m_mousePrev[button] && !m_mouseCurr[button] && !m_releaseConsumed[button];

    // 标记已消费，防止重复触发
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

// ============================================================================
// 射线检测
// ============================================================================

bool raycast(const World &world, const Camera4D &cam,
    IVec4 &hitPos, IVec4 &prevPos)
{
    constexpr double STEP = 0.2;
    constexpr double MAX_DIST = 10.0;

    Vec4 origin = cam.getPos();
    Vec4 direction = cam.getOver();  // 视线方向 = over 基向量（已归一化）

    // 初始化 prevPos 为起始点最近的格点
    IVec4 prevGrid(
        static_cast<int>(std::round(origin.x)),
        static_cast<int>(std::round(origin.y)),
        static_cast<int>(std::round(origin.z)),
        static_cast<int>(std::round(origin.w))
    );

    for (double t = STEP; t <= MAX_DIST; t += STEP)
    {
        Vec4 sample = vec4Add(origin, vec4Scale(direction, t));

        IVec4 grid(
            static_cast<int>(std::round(sample.x)),
            static_cast<int>(std::round(sample.y)),
            static_cast<int>(std::round(sample.z)),
            static_cast<int>(std::round(sample.w))
        );

        // 检查是否进入新格点
        if (grid.x == prevGrid.x && grid.y == prevGrid.y &&
            grid.z == prevGrid.z && grid.w == prevGrid.w)
        {
            continue;  // 仍在同一格点内
        }

        // 检查新格点是否有方块
        if (world.get(grid) != 0)
        {
            hitPos = grid;
            prevPos = prevGrid;
            return true;
        }

        prevGrid = grid;
    }

    return false;
}
