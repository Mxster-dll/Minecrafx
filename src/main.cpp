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
constexpr double MOVE_SPEED = 0.1;          // 键盘移动速度
constexpr double MOUSE_SENSITIVITY = 0.002; // 鼠标视角灵敏度（弧度/像素）
constexpr double ROTATE_ANGLE = 0.03;       // 键盘旋转角度（弧度）
constexpr double WHEEL_ANGLE = 0.1;         // 滚轮旋转角度（弧度/格）
constexpr double SCALE = 400.0;          // 投影缩放因子
constexpr int    FRAME_SLEEP_MS = 16;    // 约 60 FPS

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
        // 鼠标视角转动
        //   水平拖动 → rotateXW（环顾 4D 左右，保持 up=Y 不变）
        //   滚轮     → rotateZW（绕高度轴旋转 3D 切片方向）
        // ================================================================
        {
            auto [dx, dy] = input.getMouseDelta();
            if (dx != 0)
                camera.rotateXW(static_cast<double>(dx) * MOUSE_SENSITIVITY);
            // 垂直鼠标暂不绑定旋转，保持 up 恒为高度轴
            (void) dy;
        }

        // 滚轮 + Q/E：绕高度轴旋转 3D 切片（仅改变 4D 朝向，不改变位置）
        {
            int wheel = input.getMouseWheel();
            if (wheel != 0)
            {
                double angle = (wheel / static_cast<double>(WHEEL_DELTA)) * WHEEL_ANGLE;
                camera.rotateZW(angle);
            }
        }
        {
            double rotAngle = ROTATE_ANGLE;
            if (input.isKeyDown(Key::E))
                camera.rotateZW(rotAngle);
            if (input.isKeyDown(Key::Q))
                camera.rotateZW(-rotAngle);
        }

        // ================================================================
        // 移动处理（全部在 3D 切片空间内）
        //   W/S     — 沿 forward 向量（切片前后）
        //   A/D     — 沿 right   向量（切片左右）
        //   Space/Shift — 沿 up=(0,1,0,0) 向量（纯高度）
        // ================================================================
        Vec4 moveDir;

        if (input.isKeyDown(Key::W))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getForward(), MOVE_SPEED));
        if (input.isKeyDown(Key::S))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getForward(), -MOVE_SPEED));

        if (input.isKeyDown(Key::D))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getRight(), MOVE_SPEED));
        if (input.isKeyDown(Key::A))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getRight(), -MOVE_SPEED));

        if (input.isKeyDown(Key::Space))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getUp(), MOVE_SPEED));
        if (input.isKeyDown(Key::LShift) || input.isKeyDown(Key::RShift))
            moveDir = vec4Add(moveDir, vec4Scale(camera.getUp(), -MOVE_SPEED));

        if (vec4LengthSq(moveDir) > 1e-12)
            camera.move(moveDir);

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