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
 * 组合 KeyboardInput 和 MouseInput，通过窗口子类化可靠拦截 WM_MOUSEWHEEL，
 * 额外提供鼠标点击检测（上升沿触发）以及射线检测函数。
 */
class InputHandler
{
public:
    // ========================================================================
    // 迭代器支持 — 支持 range-for 语法
    // ========================================================================
    // 用法：
    //   for (auto& input : InputHandler(hwnd))
    //   {
    //       // 循环体，按下 Esc 自动退出
    //   }
    //
    // 等价于传统写法：
    //   for (InputHandler input(hwnd); !input.isPressed(Key::Esc); input.update())
    // ========================================================================

    /** @brief 结束标记类型 */
    struct Sentinel {};

    /** @brief 输入迭代器：operator++ 调用 update()，与 Sentinel 比较时检查 Esc */
    class Iterator
    {
    public:
        explicit Iterator(InputHandler &handler) : m_handler(handler) {}

        InputHandler &operator*() { return m_handler; }
        InputHandler *operator->() { return &m_handler; }

        /** @brief 前进到下一帧：调用 update() 刷新输入状态 */
        Iterator &operator++()
        {
            m_handler.update();
            return *this;
        }

        /** @brief 与 Sentinel 比较：调用 requestQuit() 时结束 */
        bool operator!=(Sentinel) const
        {
            return !m_handler.m_quitRequested;
        }

    private:
        InputHandler &m_handler;
    };

    /** @brief 获取起始迭代器 */
    Iterator begin() { return Iterator(*this); }

    /** @brief 获取结束标记 */
    Sentinel end() const { return {}; }

    // ========================================================================
    // 原有接口
    // ========================================================================

    /** @brief 构造并子类化窗口以拦截滚轮消息 */
    explicit InputHandler(HWND hwnd);

    /** @brief 析构时恢复原窗口过程 */
    ~InputHandler();

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

    /**
     * @brief 获取鼠标在窗口客户区中的屏幕坐标
     */
    POINT getMouseScreenPos() const;

    /**
     * @brief 检测鼠标按键是否被按住
     * @param button 0=左键, 1=右键, 2=中键
     */
    bool isMouseButtonDown(int button) const;

    /**
     * @brief 显示/隐藏鼠标光标
     * @param visible true=显示普通光标+解除裁剪, false=隐藏为空白光标+重新裁剪
     */
    void showMouseCursor(bool visible);

    /**
     * @brief 请求退出游戏（结束主循环）
     */
    void requestQuit() { m_quitRequested = true; }

    /**
     * @brief 是否已请求退出
     */
    bool isQuitRequested() const { return m_quitRequested; }

    /**
     * @brief 获取并清空本帧累积的鼠标滚轮增量
     * @return 滚轮增量（正值向上/远离，负值向下/靠近，WHEEL_DELTA=120）
     */
    int getMouseWheel();

private:
    HWND m_hwnd;
    KeyboardInput m_keyboard;
    MouseInput m_mouse;

    // 鼠标按键状态追踪
    static constexpr int MOUSE_BUTTON_COUNT = 3;
    bool m_mouseCurr[MOUSE_BUTTON_COUNT];
    bool m_mousePrev[MOUSE_BUTTON_COUNT];

    // 鼠标滚轮累积
    int m_wheelDelta;

    // 退出请求标志
    bool m_quitRequested = false;

    // ---- Iterator 友元（访问 m_quitRequested） ----
    friend class Iterator;

    // ---- 窗口子类化（拦截 WM_MOUSEWHEEL） ----
    WNDPROC m_oldWndProc;
    static LRESULT CALLBACK wndProcHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
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
