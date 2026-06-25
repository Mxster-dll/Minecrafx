#pragma once

#include <windows.h>
#include <utility>
#include "keyboard.h"
#include "mouse.h"

/**
 * @brief 输入处理器 — 封装键盘、鼠标状态与点击事件
 *
 * 组合 KeyboardInput 和 MouseInput，通过窗口子类化可靠拦截 WM_MOUSEWHEEL，
 * 额外提供鼠标点击检测（上升沿触发）以及射线检测函数。
 */
class InputHandler
{
public:
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
     * @brief 检测鼠标释放事件（下降沿，触发后自动清除）
     * @param button 0=左键, 1=右键
     * @return 本帧是否发生释放（按下→抬起）
     */
    bool getMouseRelease(int button);

    /** @brief 检测鼠标按键是否被按住
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
    bool m_clickConsumed[MOUSE_BUTTON_COUNT];   // 上升沿已消费（防重复触发）
    bool m_releaseConsumed[MOUSE_BUTTON_COUNT];  // 下降沿已消费

    // 鼠标滚轮累积
    int m_wheelDelta;

    // 退出请求标志
    bool m_quitRequested = false;

    // ---- 窗口子类化（拦截 WM_MOUSEWHEEL） ----
    WNDPROC m_oldWndProc;
    static LRESULT CALLBACK wndProcHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};


