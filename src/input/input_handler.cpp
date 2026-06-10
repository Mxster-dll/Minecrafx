#include "input_handler.h"
#include <cmath>

// ============================================================================
// InputHandler
// ============================================================================

InputHandler::InputHandler(HWND hwnd)
    : m_hwnd(hwnd)
    , m_keyboard(hwnd)
    , m_mouse(hwnd)
{
    for (int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
    {
        m_mouseCurr[i] = false;
        m_mousePrev[i] = false;
    }
}

void InputHandler::update()
{
    // 更新键盘
    m_keyboard.update();

    // 备份上一帧鼠标按键状态
    for (int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
        m_mousePrev[i] = m_mouseCurr[i];

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

    // 上升沿检测：当前按下且上一帧未按下
    bool clicked = m_mouseCurr[button] && !m_mousePrev[button];

    // 触发后清除当前状态，防止重复触发
    if (clicked)
        m_mouseCurr[button] = false;

    return clicked;
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
