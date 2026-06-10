#pragma once

#include <windows.h>
#include <utility>
#include "keyboard.h"
#include "mouse.h"
#include "../world.h"
#include "../camera.h"

/**
 * @brief 输入处理器 — 封装键盘、鼠标状态与点击事件
 *
 * 组合 KeyboardInput 和 MouseInput，额外提供鼠标点击检测（上升沿触发）、
 * 以及射线检测函数。
 */
class InputHandler
{
public:
    /**
     * @brief 构造输入处理器
     * @param hwnd 窗口句柄
     */
    explicit InputHandler(HWND hwnd);

    /**
     * @brief 每帧调用，刷新键盘和鼠标状态
     */
    void update();

    /**
     * @brief 按键是否被按住
     */
    bool isKeyDown(Key key) const;

    /**
     * @brief 按键是否在本帧刚按下
     */
    bool isPressed(Key key) const;

    /**
     * @brief 获取鼠标移动增量（dx, dy）
     */
    std::pair<int, int> getMouseDelta();

    /**
     * @brief 检测鼠标单击事件（上升沿，触发后自动清除）
     * @param button 0=左键, 1=右键
     * @return 本帧是否发生单击
     */
    bool getMouseClick(int button);

private:
    HWND m_hwnd;
    KeyboardInput m_keyboard;
    MouseInput m_mouse;

    // 鼠标按键状态追踪
    static constexpr int MOUSE_BUTTON_COUNT = 3;
    bool m_mouseCurr[MOUSE_BUTTON_COUNT];
    bool m_mousePrev[MOUSE_BUTTON_COUNT];
};

// ============================================================================
// 射线检测
// ============================================================================

/**
 * @brief 在 4D 世界中沿视线方向进行射线检测
 * @param world    世界数据
 * @param cam      摄像机（提供起点和 over 方向）
 * @param hitPos   输出：命中的方块坐标
 * @param prevPos  输出：命中前的采样坐标（用于放置方块）
 * @return 是否命中非空方块
 *
 * 从摄像机位置出发，沿 cam.getOver() 方向，步长 0.2，最大距离 10.0。
 * 对每个采样点四舍五入到最近整数格点，检查是否存在非空方块。
 */
bool raycast(const World &world, const Camera4D &cam,
    IVec4 &hitPos, IVec4 &prevPos);
