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
#include <ctime>

#include "linalg.h"
#include "world.h"
#include "camera.h"
#include "renderer.h"
#include "constant.h"
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
    // ---- 随机生成山脉 ----
    // 使用多层正弦波模拟自然地形
    auto terrainHeight = [](int x, int z, int w) -> int
    {
        double h = 0.0;
        h += std::sin(x * 0.25) * std::cos(z * 0.30) * 6.0;
        h += std::cos(x * 0.45 + 1.2) * std::sin(z * 0.55) * 4.0;
        h += std::sin((x + z) * 0.35) * 3.0;
        h += std::cos(w * 0.40) * std::sin(x * 0.50 + z * 0.30) * 2.5;
        h += std::sin(x * 0.70 - z * 0.60) * std::cos(w * 0.50) * 2.0;
        h += 5.0;  // 基础高度
        return (int) std::floor(h);
    };

    constexpr int MX = 24, MZ = 24, MW = 12;
    for (int x = 0; x < MX; ++x)
        for (int z = 0; z < MZ; ++z)
            for (int w = 0; w < MW; ++w)
            {
                int h = terrainHeight(x, z, w);
                if (h < 1) h = 1;
                for (int y = 0; y < h; ++y)
                    world.set(IVec4(x, y, z, w), 1);
            }

    // 移动模式：飞行 / 行走
    bool flyMode = true;
    double verticalVel = 0.0;         // 垂直速度（单位/秒）
    bool onGround = false;
    clock_t lastSpacePress = 0;
    constexpr double DOUBLE_TAP_MS = 350;

    // 物理常量（单位/秒）
    constexpr double GRAVITY = 25.0;   // 重力加速度
    constexpr double JUMP_VEL = 8.5;   // 跳跃初速度（≈1.45 方块高）
    // 切片旋转平滑变量
    double sliceVelocity = 0.0;

    clock_t lastFrame = clock();

    SetWindowText(hwnd, L"Minecrafx");

    for (auto &input : InputHandler(hwnd))
    {
        // ---- 帧间隔 ----
        clock_t nowFrame = clock();
        double dt = static_cast<double>(nowFrame - lastFrame) / CLOCKS_PER_SEC;
        lastFrame = nowFrame;
        if (dt > 0.1) dt = 0.1;
        if (dt <= 0.0) dt = 0.001;

        // 键盘控制移动
        {
            Vec4 moveDir;

            if (input.isKeyDown(Key::W))
                moveDir = vec4Add(moveDir, vec4Scale(camera.getForward(), MOVE_SPEED * dt));
            if (input.isKeyDown(Key::S))
                moveDir = vec4Add(moveDir, vec4Scale(camera.getForward(), -MOVE_SPEED * dt));
            if (input.isKeyDown(Key::D))
                moveDir = vec4Add(moveDir, vec4Scale(camera.getRight(), MOVE_SPEED * dt));
            if (input.isKeyDown(Key::A))
                moveDir = vec4Add(moveDir, vec4Scale(camera.getRight(), -MOVE_SPEED * dt));

            // ---- 模式切换 & 行走跳跃（共用空格按下事件） ----
            if (input.isPressed(Key::Space))
            {
                clock_t now = clock();
                double elapsed = static_cast<double>(now - lastSpacePress)
                    * 1000.0 / CLOCKS_PER_SEC;
                if (elapsed < DOUBLE_TAP_MS && lastSpacePress != 0)
                {
                    // 双击：切换飞行/行走模式
                    flyMode = !flyMode;
                    verticalVel = 0.0;
                    lastSpacePress = 0;
                }
                else
                {
                    // 第一击：行走模式下跳跃
                    lastSpacePress = now;
                    if (!flyMode && onGround)
                    {
                        verticalVel = JUMP_VEL;
                        onGround = false;
                    }
                }
            }

            // ---- Y 轴移动 ----
            double desiredDY = 0.0;
            if (flyMode)
            {
                if (input.isKeyDown(Key::Space))
                    desiredDY = MOVE_SPEED * dt;
                if (input.isKeyDown(Key::LShift) || input.isKeyDown(Key::RShift))
                    desiredDY = -MOVE_SPEED * dt;
            }
            else
            {
                verticalVel -= GRAVITY * dt;
                desiredDY = verticalVel * dt;
            }

            moveDir.y += desiredDY;

            if (vec4LengthSq(moveDir) > 1e-12)
            {
                // ---- 3D 棱柱碰撞检测（圆柱体：半径0.8 xzw，高1.6 Y，摄像机在顶部） ----
                const Vec4 &camPos = camera.getPos();
                double half = 0.5, cR = CYLINDER_R, cH = CYLINDER_H;

                Plane2D plane = camera.getViewPlane();
                double nAbs = std::abs(plane.n.x) + std::abs(plane.n.z) + std::abs(plane.n.w);

                auto check3D = [&](const Vec4 &test) -> bool
                {
                    double sr = cR + half + 1.0;
                    int minX = (int) std::floor(test.x - sr);
                    int maxX = (int) std::floor(test.x + sr);
                    int minY = (int) std::floor((test.y - cH) - half);
                    int maxY = (int) std::floor(test.y + half);
                    int minZ = (int) std::floor(test.z - sr);
                    int maxZ = (int) std::floor(test.z + sr);
                    int minW = (int) std::floor(test.w - sr);
                    int maxW = (int) std::floor(test.w + sr);

                    for (int bx = minX; bx <= maxX; ++bx)
                        for (int by = minY; by <= maxY; ++by)
                            for (int bz = minZ; bz <= maxZ; ++bz)
                                for (int bw = minW; bw <= maxW; ++bw)
                                {
                                    if (!world.get(IVec4(bx, by, bz, bw))) continue;

                                    double cx = bx - camPos.x, cz = bz - camPos.z, cw = bw - camPos.w;
                                    double pd = std::abs(plane.n.x * cx + plane.n.z * cz + plane.n.w * cw);
                                    if (pd > half * nAbs + cR) continue;

                                    // 方块 AABB
                                    double bXLo = bx - half, bXHi = bx + half;
                                    double bZLo = bz - half, bZHi = bz + half;
                                    double bWLo = bw - half, bWHi = bw + half;
                                    double bYLo = by - half, bYHi = by + half;

                                    // 圆柱体碰撞：xzw 球体半径 cR，Y 范围 [test.y-cH, test.y]
                                    double pXLo = test.x - cR, pXHi = test.x + cR;
                                    double pZLo = test.z - cR, pZHi = test.z + cR;
                                    double pWLo = test.w - cR, pWHi = test.w + cR;
                                    double pYLo = test.y - cH, pYHi = test.y;

                                    if (pXLo < bXHi && pXHi > bXLo &&
                                        pZLo < bZHi && pZHi > bZLo &&
                                        pWLo < bWHi && pWHi > bWLo &&
                                        pYLo < bYHi && pYHi > bYLo)
                                        return true;
                                }
                    return false;
                };

                // 逐轴滑动（x/z/w/y — 适配 xzw 空间 AABB，实现贴墙滑动）
                Vec4 newPos = camPos;

                if (std::abs(moveDir.x) > 1e-12)
                {
                    Vec4 t = newPos; t.x += moveDir.x;
                    if (!check3D(t)) newPos = t;
                }
                if (std::abs(moveDir.z) > 1e-12)
                {
                    Vec4 t = newPos; t.z += moveDir.z;
                    if (!check3D(t)) newPos = t;
                }
                if (std::abs(moveDir.w) > 1e-12)
                {
                    Vec4 t = newPos; t.w += moveDir.w;
                    if (!check3D(t)) newPos = t;
                }
                if (std::abs(moveDir.y) > 1e-12)
                {
                    Vec4 t = newPos; t.y += moveDir.y;
                    if (!check3D(t)) { newPos = t; if (!flyMode) onGround = false; }
                    else if (!flyMode && moveDir.y < 0.0) onGround = true;  // 向下被阻挡 → 着地
                }
                else if (!flyMode && verticalVel <= 0.0)
                {
                    // 无 Y 移动且速度向下：检测是否着地
                    Vec4 t = newPos; t.y -= 0.001;
                    onGround = check3D(t);
                }

                Vec4 actual = vec4Sub(newPos, camPos);
                if (vec4LengthSq(actual) > 1e-12) camera.move(actual);

                // 着地时清零垂直速度
                if (onGround && verticalVel < 0.0) verticalVel = 0.0;
            }
        }

        // 鼠标控制视角旋转
        {
            auto [dx, dy] = input.getMouseDelta();

            // 水平分量控制 i, j 在切片平面内旋转，不改变切片平面的位置
            if (dx != 0) camera.rotateAroundUp(static_cast<double>(dx) *MOUSE_SENSITIVITY);

            // 垂直分量控制俯仰角，仅改变视角与 XZW 平面的夹角，同样不改变切片平面的位置
            if (dy != 0) camera.addPitch(static_cast<double>(-dy) *MOUSE_SENSITIVITY);
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
