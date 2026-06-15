/**
 * @file main.cpp
 * @author 陈煜仕 (Mxster@qq.com)
 * @brief Minecrafx 主入口
 * @version 3.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 * 初始化窗口、世界、摄像机、渲染器
 * 处理主循环的移动、旋转、方块交互和渲染
 */

#include <graphics.h>
#include <windows.h>
#include <imm.h>
#include <cmath>

#include "linalg.h"
#include "world.h"
#include "camera.h"
#include "renderer.h"
#include "input/input_handler.h"
#include "constant.h"
int main()
{
    initgraph(SCREEN_WIDTH, SCREEN_HEIGHT);
    HWND hwnd = GetHWnd();

    // 禁用输入法，防止 Shift 被系统拦截切换中英文
    ImmAssociateContext(hwnd, NULL);

    BeginBatchDraw();

    World world;
    Camera4D camera;
    Renderer renderer(SCREEN_WIDTH, SCREEN_HEIGHT);

    // 放置一些初始方块用于测试
    // 放置 16×3×16×16 随机着色地板（y=0~2，使用哈希过程色）
    for (int x = 0; x < 16; ++x)
        for (int y = 0; y < 3; ++y)
            for (int z = 0; z < 16; ++z)
                for (int w = 0; w < 16; ++w)
                    world.set(IVec4(x, y, z, w), 1);

    // 切片旋转平滑变量（无回弹：速度直接追踪瞬时输入）
    double sliceVelocity = 0.0;  // 当前旋转速度

    SetWindowText(hwnd, L"Minecrafx");

    for (auto &input : InputHandler(hwnd))
    {
        // 键盘控制移动
        {
            Vec4 moveDir;

            if (input.isKeyDown(Key::W))
                moveDir = vec4Add(moveDir, vec4Scale(camera.getForward(), MOVE_SPEED));
            if (input.isKeyDown(Key::S))
                moveDir = vec4Add(moveDir, vec4Scale(camera.getForward(), -MOVE_SPEED));
            if (input.isKeyDown(Key::D))
                moveDir = vec4Add(moveDir, vec4Scale(camera.getRight(), MOVE_SPEED));
            if (input.isKeyDown(Key::A))
                moveDir = vec4Add(moveDir, vec4Scale(camera.getRight(), -MOVE_SPEED));

            // 上下始终沿世界高度轴 Y
            if (input.isKeyDown(Key::Space))
                moveDir = vec4Add(moveDir, Vec4(0.0, MOVE_SPEED, 0.0, 0.0));
            if (input.isKeyDown(Key::LShift) || input.isKeyDown(Key::RShift))
                moveDir = vec4Add(moveDir, Vec4(0.0, -MOVE_SPEED, 0.0, 0.0));

            if (vec4LengthSq(moveDir) > 1e-12)
                camera.move(moveDir);
        }

        // 鼠标控制视角旋转
        {
            auto [dx, dy] = input.getMouseDelta();

            // 水平分量控制 i, j 在切片平面内旋转，不改变切片平面的位置
            if (dx != 0) camera.rotateAroundUp(static_cast<double>(dx) * MOUSE_SENSITIVITY);

            // 垂直分量控制俯仰角，仅改变视角与 XZW 平面的夹角，同样不改变切片平面的位置
            if (dy != 0) camera.addPitch(static_cast<double>(-dy) * MOUSE_SENSITIVITY);
        }

        // 滚轮 / Q/E → rotateSlice（绕 j=right 轴旋转切片平面）
        {
            // 本帧的期望输入（滚轮 + Q/E，统一步长 SLICE_STEP）
            double inputDesire = 0.0;

            int wheel = input.getMouseWheel();
            if (wheel != 0)
                inputDesire += (wheel / static_cast<double>(WHEEL_DELTA)) * SLICE_STEP;

            if (input.isKeyDown(Key::E))
                inputDesire += SLICE_STEP;
            if (input.isKeyDown(Key::Q))
                inputDesire -= SLICE_STEP;

            // 速度平滑趋近瞬时输入，松开后自然衰减至零
            sliceVelocity += (inputDesire - sliceVelocity) * SLICE_SMOOTH;

            if (std::abs(sliceVelocity) > 1e-10)
            {
                camera.rotateSlice(sliceVelocity);
            }
            else
            {
                sliceVelocity = 0.0;
            }
        }

        // 视角重置（调试）
        if (input.isPressed(Key::R)) camera.reset();


        // 鼠标按键行为
        {
            IVec4 hitPos, prevPos;

            // 左键：破坏方块
            if (input.getMouseClick(0))
            {
                if (raycast(world, camera, hitPos, prevPos))
                    world.set(hitPos, 0);
            }

            // 右键：放置方块
            if (input.getMouseClick(1))
            {
                if (raycast(world, camera, hitPos, prevPos))
                    world.set(prevPos, 1);
            }
        }

        // 绘制
        {
            cleardevice();
            setbkcolor(RGB(10, 10, 30));

            renderer.renderWorld(world, camera);
            renderer.drawCrosshair();
            renderer.drawHUD(camera);

            FlushBatchDraw();
        }
    }

    EndBatchDraw();
    closegraph();
}