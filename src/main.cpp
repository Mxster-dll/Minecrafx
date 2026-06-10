#include <graphics.h>
#include "input/mouse.h"
#include "input/keyboard.h"

int main()
{
    initgraph(800, 600);

    HWND hwnd = GetHWnd();

    MouseInput mouse(hwnd);
    KeyboardInput keyboard(hwnd);

    for (;;)
    {
        keyboard.update();

        if (keyboard.isPressed(Key::Esc)) break;

        auto [dx, dy] = mouse.getDelta();

        // 主逻辑

        Sleep(10);
    }

    closegraph();
}