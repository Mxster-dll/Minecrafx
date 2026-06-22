#include "mouse.h"

static HCURSOR createBlankCursor()
{
    unsigned char andMask[] = { 0x00 };
    unsigned char xorMask[] = { 0x00 };
    return CreateCursor(GetModuleHandle(NULL), 0, 0, 1, 1,
        (void *) andMask, (void *) xorMask);
}

static POINT getWindowCenter(HWND hwnd)
{
    RECT rect;
    GetClientRect(hwnd, &rect);
    POINT center;
    center.x = (rect.left + rect.right) / 2;
    center.y = (rect.top + rect.bottom) / 2;
    ClientToScreen(hwnd, &center);
    return center;
}

MouseInput::MouseInput(HWND hwnd, int margin)
    : m_hwnd(hwnd)
{
    // 计算限制矩形（屏幕坐标，带边距）
    RECT rect;
    GetClientRect(hwnd, &rect);

    POINT pt1 = { rect.left + margin, rect.top + margin };
    POINT pt2 = { rect.right - margin, rect.bottom - margin };

    ClientToScreen(hwnd, &pt1);
    ClientToScreen(hwnd, &pt2);

    m_clipRect = { pt1.x, pt1.y, pt2.x, pt2.y };

    // 替换窗口类光标
    m_hBlankCursor = createBlankCursor();
    m_hOldCursor = (HCURSOR) SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR) m_hBlankCursor);
    SetCursor(m_hBlankCursor);

    // 限制鼠标在窗口内
    ClipCursor(&m_clipRect);

    // 初始化鼠标位置（复位到窗口中心）
    m_center = getWindowCenter(hwnd);
    m_lastPos = m_center;
    SetCursorPos(m_center.x, m_center.y);
}

MouseInput::~MouseInput()
{
    ClipCursor(NULL);
    SetClassLongPtr(m_hwnd, GCLP_HCURSOR, (LONG_PTR) m_hOldCursor);
    if (m_hBlankCursor)
        DestroyCursor(m_hBlankCursor);
}

std::pair<int, int> MouseInput::getDelta()
{
    POINT curPos;
    GetCursorPos(&curPos);
    int dx = curPos.x - m_lastPos.x;
    int dy = curPos.y - m_lastPos.y;

    // 复位到中心
    m_center = getWindowCenter(m_hwnd);
    SetCursorPos(m_center.x, m_center.y);
    m_lastPos = m_center;

    // 强制空光标，防止移动到边缘时系统还原
    SetCursor(m_hBlankCursor);

    return { dx, dy };
}

void MouseInput::showCursor()
{
    // 恢复原窗口类光标
    SetClassLongPtr(m_hwnd, GCLP_HCURSOR, (LONG_PTR) m_hOldCursor);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    ShowCursor(TRUE);
    ClipCursor(NULL);
}

void MouseInput::hideCursor()
{
    ShowCursor(FALSE);
    SetClassLongPtr(m_hwnd, GCLP_HCURSOR, (LONG_PTR) m_hBlankCursor);
    SetCursor(m_hBlankCursor);
    ClipCursor(&m_clipRect);

    // 复位鼠标到窗口中心
    m_center = getWindowCenter(m_hwnd);
    m_lastPos = m_center;
    SetCursorPos(m_center.x, m_center.y);
}
