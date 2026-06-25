#pragma once

#include <windows.h>
#include <utility>

class MouseInput
{
public:

    explicit MouseInput(HWND hwnd, int margin = 10);

    ~MouseInput();

    std::pair<int, int> getDelta();

    void showCursor();

    void hideCursor();

private:
    HWND m_hwnd;
    HCURSOR m_hBlankCursor;
    HCURSOR m_hOldCursor;
    RECT m_clipRect;
    POINT m_center;
    POINT m_lastPos;
};