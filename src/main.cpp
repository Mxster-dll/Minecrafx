/**
 * @file main.cpp
 * @brief 4D Miner — 四维空间体素沙盒（EasyX 版）主入口
 *
 * 初始化窗口、世界、摄像机、渲染器和输入系统，
 * 进入主循环处理移动、旋转、方块交互和渲染。
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

 // ============================================================================
 // 常量
 // ============================================================================
constexpr int    SCREEN_WIDTH = 800;
constexpr int    SCREEN_HEIGHT = 600;
constexpr double MOVE_SPEED = 0.1;           // 键盘移动速度（单位/帧）
constexpr double MOUSE_SENSITIVITY = 0.003;  // 鼠标视角灵敏度（弧度/像素）
constexpr double ROTATE_ANGLE = 0.03;        // 键盘旋转角度（弧度/帧）
constexpr double SCALE = 400.0;              // 投影缩放因子
constexpr int    FRAME_SLEEP_MS = 16;        // 约 60 FPS
constexpr double SLICE_STEP = 0.03;         // 切片旋转每格步长（弧度），滚轮/Q/E 统一
constexpr double SLICE_SMOOTH = 0.28;       // 速度平滑系数（0~1，越大越快）

// ============================================================================
// 主函数
// ============================================================================
int main()
{
    // ---- 初始化图形窗口与双缓冲 ----
    initgraph(SCREEN_WIDTH, SCREEN_HEIGHT);
    HWND hwnd = GetHWnd();

    // 禁用输入法：防止 Shift 等键被系统拦截切换中英文
    ImmAssociateContext(hwnd, NULL);

    BeginBatchDraw();

    // ---- 创建核心对象 ----
    World world;
    // 3×3×3×3 实心方块：中心 (1,1,1,-5) 在切片平面上，四面八方全包围 → 红色
    for (int x = 0; x <= 2; ++x)
        for (int y = 0; y <= 2; ++y)
            for (int z = 0; z <= 2; ++z)
                for (int w = -6; w <= -4; ++w)
                    world.set(IVec4(x, y, z, w), 1);

    Camera4D camera;
    Renderer renderer(SCREEN_WIDTH, SCREEN_HEIGHT, SCALE);
    InputHandler input(hwnd);

    // 切片旋转平滑变量（无回弹：速度直接追踪瞬时输入）
    double sliceVelocity = 0.0;  // 当前旋转速度

    SetWindowText(hwnd, L"4D Miner — 四维体素沙盒");

    // ---- 主循环 ----
    bool running = true;
    while (running)
    {
        // 1. 更新输入状态
        input.update();

        // 退出
        if (input.isPressed(Key::Esc))
        {
            running = false;
            break;
        }

        // ---- 1. 键盘移动 ----
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

        // ---- 2. 键盘旋转 ----
        {
            if (input.isKeyDown(Key::I)) camera.rotateXY(ROTATE_ANGLE);
            if (input.isKeyDown(Key::J)) camera.rotateXZ(ROTATE_ANGLE);
            if (input.isKeyDown(Key::K)) camera.rotateYZ(ROTATE_ANGLE);
            if (input.isKeyDown(Key::L)) camera.rotateXW(ROTATE_ANGLE);
            if (input.isKeyDown(Key::U)) camera.rotateYW(ROTATE_ANGLE);
            if (input.isKeyDown(Key::O)) camera.rotateZW(ROTATE_ANGLE);
        }

        // ---- 3. 鼠标视角旋转 ----
        //   水平 → rotateAroundUp（yaw：i,j 在切片平面内旋转，不动 i,j,n 的 XZW 方向）
        //   垂直 → addPitch（俯仰角：仅改变视角与 XZW 平面的夹角，不动 i,j,n）
        {
            auto [dx, dy] = input.getMouseDelta();
            if (dx != 0)
                camera.rotateAroundUp(static_cast<double>(dx) * MOUSE_SENSITIVITY);
            if (dy != 0)
                camera.addPitch(static_cast<double>(-dy) * MOUSE_SENSITIVITY);
        }

        // ---- 4. 滚轮 / Q/E → rotateSlice（绕 j=right 轴旋转切片平面） ----
        //     无回弹：瞬时输入直接驱动目标速度，速度平滑追踪
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

            // 速度平滑趋近瞬时输入（松开后自然衰减至零，无回弹）
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

        // ---- 4. 视角重置 ----
        if (input.isPressed(Key::R))
        {
            camera.reset();
        }

        // ---- 5. 鼠标交互 ----
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

        // ---- 6. 渲染 ----
        cleardevice();

        setbkcolor(RGB(10, 10, 30));
        renderer.renderWorld(world, camera);
        renderer.drawCrosshair();
        renderer.drawHUD(camera);

        FlushBatchDraw();
        Sleep(FRAME_SLEEP_MS);
    }

    // ---- 清理 ----
    EndBatchDraw();
    closegraph();
    return 0;
}