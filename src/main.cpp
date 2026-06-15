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
            {
                // ---- 3D 棱柱碰撞检测 ----
                // 渲染管线：4D 方块 → xzw 立方体 ∩ 观察平面 → UV 多边形 → 沿 Y 挤出 → 3D 棱柱
                // 碰撞在此 3D 空间 (U,V,Y) 中进行，玩家位于原点
                Plane2D plane = camera.getViewPlane();
                const Vec4 &camPos = camera.getPos();
                double half = 0.5;  // BLOCK_HALF
                double r = PLAYER_R;

                // 基向量分量绝对值（投影半宽计算用）
                double pAbs = std::abs(plane.p.x) + std::abs(plane.p.z) + std::abs(plane.p.w);
                double qAbs = std::abs(plane.q.x) + std::abs(plane.q.z) + std::abs(plane.q.w);
                double nAbs = std::abs(plane.n.x) + std::abs(plane.n.z) + std::abs(plane.n.w);

                auto check3D = [&](const Vec4 &test) -> bool
                {
                    // 玩家在相机相对 3D 空间中的位置
                    double rx = test.x - camPos.x, rz = test.z - camPos.z;
                    double rw = test.w - camPos.w, ry = test.y - camPos.y;
                    double pU = rx * plane.p.x + rz * plane.p.z + rw * plane.p.w;
                    double pV = rx * plane.q.x + rz * plane.q.z + rw * plane.q.w;
                    double pY = ry;

                    double sr = r + half * (pAbs > qAbs ? pAbs : qAbs) + 1.0;
                    int minX = (int) std::floor((test.x - sr));
                    int maxX = (int) std::floor((test.x + sr));
                    int minY = (int) std::floor((test.y - sr));
                    int maxY = (int) std::floor((test.y + sr));
                    int minZ = (int) std::floor((test.z - sr));
                    int maxZ = (int) std::floor((test.z + sr));
                    int minW = (int) std::floor((test.w - sr));
                    int maxW = (int) std::floor((test.w + sr));

                    for (int bx = minX; bx <= maxX; ++bx)
                        for (int by = minY; by <= maxY; ++by)
                            for (int bz = minZ; bz <= maxZ; ++bz)
                                for (int bw = minW; bw <= maxW; ++bw)
                                {
                                    if (!world.get(IVec4(bx, by, bz, bw))) continue;

                                    double cx = bx - camPos.x, cz = bz - camPos.z;
                                    double cw = bw - camPos.w, cy = by - camPos.y;

                                    // 平面相交检测
                                    double pd = std::abs(plane.n.x * cx + plane.n.z * cz + plane.n.w * cw);
                                    if (pd > half * nAbs + r) continue;

                                    // 方块 3D AABB
                                    double uC = cx * plane.p.x + cz * plane.p.z + cw * plane.p.w;
                                    double vC = cx * plane.q.x + cz * plane.q.z + cw * plane.q.w;
                                    double uH = half * pAbs, vH = half * qAbs;

                                    double cu = (pU < uC - uH) ? (uC - uH) : (pU > uC + uH) ? (uC + uH) : pU;
                                    double cv = (pV < vC - vH) ? (vC - vH) : (pV > vC + vH) ? (vC + vH) : pV;
                                    double cY = (pY < cy - half) ? (cy - half) : (pY > cy + half) ? (cy + half) : pY;

                                    double du = pU - cu, dv = pV - cv, dy = pY - cY;
                                    if (du * du + dv * dv + dy * dy <= r * r) return true;
                                }
                    return false;
                };

                // 逐方向滑动（forward / right / Y）
                Vec4 newPos = camPos;
                const Vec4 &fwd = camera.getForward(), &rht = camera.getRight();

                double fComp = vec4Dot(moveDir, fwd);
                if (std::abs(fComp) > 1e-12)
                {
                    Vec4 t = vec4Add(newPos, vec4Scale(fwd, fComp));
                    if (!check3D(t)) newPos = t;
                }
                double rComp = vec4Dot(moveDir, rht);
                if (std::abs(rComp) > 1e-12)
                {
                    Vec4 t = vec4Add(newPos, vec4Scale(rht, rComp));
                    if (!check3D(t)) newPos = t;
                }
                if (std::abs(moveDir.y) > 1e-12)
                {
                    Vec4 t = newPos; t.y += moveDir.y;
                    if (!check3D(t)) newPos = t;
                }

                Vec4 actual = vec4Sub(newPos, camPos);
                if (vec4LengthSq(actual) > 1e-12) camera.move(actual);
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