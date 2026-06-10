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
    // ---- w=-5 层：5×5 地板 + 四柱 ----
    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 2; ++z)
            if (x != 0 || z != 0)
                world.set(IVec4(x, 0, z, -5), 1);
    world.set(IVec4(-2, 1, -2, -5), 1); world.set(IVec4(2, 1, -2, -5), 1);
    world.set(IVec4(-2, 1, 2, -5), 1); world.set(IVec4(2, 1, 2, -5), 1);
    world.set(IVec4(-2, 2, -2, -5), 1); world.set(IVec4(2, 2, -2, -5), 1);
    world.set(IVec4(-2, 2, 2, -5), 1); world.set(IVec4(2, 2, 2, -5), 1);

    // ---- w=0 层：3×3 小平台 ----
    for (int x = -1; x <= 1; ++x)
        for (int z = -1; z <= 1; ++z)
            world.set(IVec4(x, 0, z, 0), 1);

    // ---- w=3 层：中心单柱 ----
    world.set(IVec4(0, 0, 0, 3), 1);
    world.set(IVec4(0, 1, 0, 3), 1);
    world.set(IVec4(0, 2, 0, 3), 1);

    // ---- 第四维轴线：沿 W 轴放标记块 ----
    world.set(IVec4(0, -1, 0, -3), 1);
    world.set(IVec4(0, -1, 0, 3), 1);
    world.set(IVec4(0, -1, 0, -5), 1);
    world.set(IVec4(0, -1, 0, 0), 1);

    Camera4D camera;
    Renderer renderer(SCREEN_WIDTH, SCREEN_HEIGHT, SCALE);
    InputHandler input(hwnd);

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
        {
            int wheel = input.getMouseWheel();
            if (wheel != 0)
            {
                double angle = (wheel / static_cast<double>(WHEEL_DELTA)) * 0.5;
                camera.rotateSlice(angle);
            }
        }
        {
            double rotAngle = ROTATE_ANGLE;
            if (input.isKeyDown(Key::E))
                camera.rotateSlice(rotAngle);
            if (input.isKeyDown(Key::Q))
                camera.rotateSlice(-rotAngle);
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