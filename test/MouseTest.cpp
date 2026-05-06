#include <graphics.h>
#include <windows.h>
#include <iostream>
using std::cout;
const char endl = '\n';

HCURSOR CreateBlankCursor()
{
    unsigned char andMask[] = { 0x00 };
    unsigned char xorMask[] = { 0x00 };
    HCURSOR hCur = CreateCursor(GetModuleHandle(NULL), 0, 0, 1, 1,
        (void *) andMask, (void *) xorMask);
    return hCur;
}

POINT GetWindowCenter(HWND hwnd)
{
    RECT rect;
    GetClientRect(hwnd, &rect);
    POINT center;
    center.x = (rect.left + rect.right) / 2;
    center.y = (rect.top + rect.bottom) / 2;
    ClientToScreen(hwnd, &center);
    return center;
}

int main()
{
    initgraph(800, 600);
    HWND hwnd = GetHWnd();

    // 获取窗口客户区屏幕矩形（用于限制鼠标）
    RECT rect;
    GetClientRect(hwnd, &rect);
    POINT pt1 = { rect.left + 10, rect.top + 10 };
    POINT pt2 = { rect.right - 10, rect.bottom - 10 };
    ClientToScreen(hwnd, &pt1);
    ClientToScreen(hwnd, &pt2);
    RECT clientRect = { pt1.x, pt1.y, pt2.x, pt2.y };

    // 创建空光标并替换窗口类光标
    HCURSOR hBlankCursor = CreateBlankCursor();
    HCURSOR hOldCursor = (HCURSOR) SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR) hBlankCursor);
    SetCursor(hBlankCursor);

    // 限制鼠标在窗口内
    ClipCursor(&clientRect);

    // 初始化鼠标位置（复位到窗口中心）
    POINT center = GetWindowCenter(hwnd);
    POINT lastPos = center;
    SetCursorPos(center.x, center.y);

    // 主循环
    ExMessage msg;
    bool running = true;
    while (running)
    {
        // 处理消息（ESC 退出）
        while (peekmessage(&msg, EX_KEY | EX_MOUSE))
        {
            if (msg.message == WM_KEYDOWN && msg.vkcode == VK_ESCAPE)
                running = false;
        }

        // 获取鼠标移动差值
        POINT curPos;
        GetCursorPos(&curPos);
        int dx = curPos.x - lastPos.x;
        int dy = curPos.y - lastPos.y;

        if (dx != 0 || dy != 0)
        {
            cout << "Mouse delta: dx=" << dx << ", dy=" << dy << endl;
        }

        // 复位鼠标到窗口中心
        center = GetWindowCenter(hwnd);
        SetCursorPos(center.x, center.y);
        lastPos = center;

        // ⭐ 关键：强制设置光标为空光标，防止移动到窗口边缘时系统改变光标
        SetCursor(hBlankCursor);

        Sleep(10);
    }

    // 清理
    ClipCursor(NULL);
    SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR) hOldCursor);
    DestroyCursor(hBlankCursor);
    closegraph();
    return 0;
}