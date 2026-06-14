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
#include "superblock.h"

 // ============================================================================
 // 常量
 // ============================================================================
constexpr int    SCREEN_WIDTH = 800;
constexpr int    SCREEN_HEIGHT = 600;
constexpr double MOVE_SPEED = 0.1;           // 键盘移动速度（单位/帧）
constexpr double MOUSE_SENSITIVITY = 0.003;  // 鼠标视角灵敏度（弧度/像素）
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
    Camera4D camera;
    Renderer renderer(SCREEN_WIDTH, SCREEN_HEIGHT);

    // 放置一个超方块（16×16×16×16 子方块），贴图着色
    renderer.addSuperBlock(SuperBlock(IVec4(0, 1, -3, 0)));
    renderer.loadTextures(L"D:/Project/Ongoing/Minecrafx/assert/texture/grass_block");

    // 切片旋转平滑变量（无回弹：速度直接追踪瞬时输入）
    double sliceVelocity = 0.0;  // 当前旋转速度

    SetWindowText(hwnd, L"4D Miner — 四维体素沙盒");

    for (InputHandler input(hwnd); !input.isPressed(Key::Esc); input.update())
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
    }

    // ---- 清理 ----
    EndBatchDraw();
    closegraph();
    return 0;
}