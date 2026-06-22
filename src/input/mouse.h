#pragma once

#include <windows.h>
#include <utility>

/**
 * @brief 鼠标输入管理器（RAII）
 *
 * 将系统鼠标隐藏并锁定在窗口内，每帧返回鼠标移动增量（dx, dy），
 * 析构时自动恢复光标和裁剪区域
 */
class MouseInput
{
public:
    /**
     * @brief 初始化鼠标捕获
     * @param hwnd    窗口句柄
     * @param margin  鼠标活动边距（像素），默认 10
     */
    explicit MouseInput(HWND hwnd, int margin = 10);

    /**
     *  @brief 析构时自动恢复光标、解除裁剪、销毁空光标
     */
    ~MouseInput();

    /**
     * @brief 获取本帧鼠标移动增量并复位鼠标到窗口中心
     * @return std::pair<int, int> { dx, dy }，支持结构化绑定 auto [dx, dy] = ...
     */
    std::pair<int, int> getDelta();

    /** @brief 显示普通鼠标光标（恢复原窗口类光标，解除裁剪） */
    void showCursor();

    /** @brief 隐藏鼠标光标（设为空白光标，重新裁剪到窗口内） */
    void hideCursor();

private:
    HWND m_hwnd;
    HCURSOR m_hBlankCursor;
    HCURSOR m_hOldCursor;
    RECT m_clipRect;
    POINT m_center;
    POINT m_lastPos;
};
