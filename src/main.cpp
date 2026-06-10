/**
 * @file main.cpp
 * @brief 4D Miner — 四维空间体素沙盒（EasyX 版）主入口
 *
 * 初始化窗口、世界、摄像机、渲染器和输入系统，
 * 进入主循环处理移动、旋转、方块交互和渲染。
 */

#include <graphics.h>
#include <windows.h>
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
constexpr double MOVE_SPEED = 0.1;    // 每帧移动速度
constexpr double ROTATE_ANGLE = 0.03;   // 每帧旋转角度（弧度）
constexpr double SCALE = 400.0;  // 投影缩放因子
constexpr int    FRAME_SLEEP_MS = 16;     // 约 60 FPS

// ============================================================================
// 主函数
// ============================================================================
int main()
{
    // ---- 初始化图形窗口与双缓冲 ----
    initgraph(SCREEN_WIDTH, SCREEN_HEIGHT);
    HWND hwnd = GetHWnd();
    BeginBatchDraw();

    // ---- 创建核心对象 ----
    World world;
    generateFloor(world);

    Camera4D camera;
    Renderer renderer(SCREEN_WIDTH, SCREEN_HEIGHT, SCALE);
    InputHandler input(hwnd);

    // 用 SetWindowText 设置窗口标题
    SetWindowText(hwnd, L"4D Miner — 四维体素沙盒");

    // ---- 主循环 ----
    bool running = true;
    while (running)
    {
        // --- 更新输入状态 ---
        input.update();

        // --- 退出 ---
        if (input.isPressed(Key::Esc))
        {
            running = false;
            break;
        }

        // --- 摄像机复位 ---
        if (input.isPressed(Key::R))
        {
            camera.reset();
        }

        // ================================================================
        // 移动处理
        // ================================================================
        Vec4 moveDir;  // 累积移动方向

        // 前后移动：沿 forward 向量
        if (input.isKeyDown(Key::W))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getForward(), MOVE_SPEED));
        if (input.isKeyDown(Key::S))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getForward(), -MOVE_SPEED));

        // 左右移动：沿 right 向量
        if (input.isKeyDown(Key::D))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getRight(), MOVE_SPEED));
        if (input.isKeyDown(Key::A))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getRight(), -MOVE_SPEED));

        // 上下移动：沿 up 向量
        if (input.isKeyDown(Key::Space))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getUp(), MOVE_SPEED));
        if (input.isKeyDown(Key::LShift) || input.isKeyDown(Key::RShift))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getUp(), -MOVE_SPEED));

        // 第四维移动：沿 over 向量
        if (input.isKeyDown(Key::E))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getOver(), MOVE_SPEED));
        if (input.isKeyDown(Key::Q))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getOver(), -MOVE_SPEED));

        // 应用移动
        if (vec4LengthSq(moveDir) > 1e-12)
            camera.move(moveDir);

        // ================================================================
        // 旋转处理（六种平面旋转，Shift 反转方向）
        // ================================================================
        double rotAngle = ROTATE_ANGLE;
        bool shift = input.isKeyDown(Key::LShift) || input.isKeyDown(Key::RShift);

        // 用 isPressed 检测单次触发（也可以持续按住旋转，用 isKeyDown）
        if (input.isKeyDown(Key::Num1))
            camera.rotateXY(shift ? -rotAngle : rotAngle);
        if (input.isKeyDown(Key::Num2))
            camera.rotateXZ(shift ? -rotAngle : rotAngle);
        if (input.isKeyDown(Key::Num3))
            camera.rotateYZ(shift ? -rotAngle : rotAngle);
        if (input.isKeyDown(Key::Num4))
            camera.rotateXW(shift ? -rotAngle : rotAngle);
        if (input.isKeyDown(Key::Num5))
            camera.rotateYW(shift ? -rotAngle : rotAngle);
        if (input.isKeyDown(Key::Num6))
            camera.rotateZW(shift ? -rotAngle : rotAngle);

        // ================================================================
        // 方块交互（鼠标点击）
        // ================================================================
        IVec4 hitPos, prevPos;

        // 左键：破坏方块
        if (input.getMouseClick(0))
        {
            if (raycast(world, camera, hitPos, prevPos))
            {
                world.set(hitPos, 0);  // 设为空气
            }
        }

        // 右键：放置方块
        if (input.getMouseClick(1))
        {
            if (raycast(world, camera, hitPos, prevPos))
            {
                // 在命中前的位置放置方块（防止放置在摄像机内部）
                world.set(prevPos, 1);
            }
        }

        // ================================================================
        // 渲染
        // ================================================================
        cleardevice();

        // 背景色（深空蓝黑）
        setbkcolor(RGB(10, 10, 30));

        // 渲染世界线框
        renderer.renderWorld(world, camera);

        // 十字准星
        renderer.drawCrosshair();

        // HUD 信息
        renderer.drawHUD(camera);

        // 刷新双缓冲
        FlushBatchDraw();

        // 帧率控制
        Sleep(FRAME_SLEEP_MS);
    }

    // ---- 清理 ----
    EndBatchDraw();
    closegraph();
    return 0;
}