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

 // 方块过程色（哈希）
static COLORREF blockColor(int x, int y, int z, int w)
{
    unsigned int h = (unsigned int) (x * 73856093 + y * 19349663 + z * 83492791 + w * 39916801);
    h = (h ^ (h >> 13)) * 0x9e3779b9;
    int r = 60 + (h & 0xFF) % 156;
    int g = 60 + ((h >> 8) & 0xFF) % 156;
    int b = 60 + ((h >> 16) & 0xFF) % 156;
    return RGB(r, g, b);
}
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
    double verticalVel = 0.0;
    bool onGround = false;
    clock_t lastSpacePress = 0;
    constexpr double DOUBLE_TAP_MS = 350;
    constexpr double GRAVITY = 25.0;
    constexpr double JUMP_VEL = 8.5;
    double sliceVelocity = 0.0;
    clock_t lastFrame = clock();

    // ---- 3D 地图 + 3D 摄像机 ----
    Map3D map3D;
    double cam3U = 0, cam3V = 0, cam3Y = 0;   // 3D 位置（地图坐标系）
    double cam3Yaw = 0, cam3Pitch = 0;          // 3D 朝向

    // 初次生成地图
    map3D = generateMap3D(world, camera, 0.5,
        [](int bx, int by, int bz, int bw) -> COLORREF
    {
        return blockColor(bx, by, bz, bw);
    });
    // 从 4D 相机计算 3D 初始位置
    {
        Plane2D pl = camera.getViewPlane();
        Vec3 cXZW = Vec3::fromVec4(camera.getPos());
        cam3U = vec3Dot(cXZW, pl.p) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.p);
        cam3V = vec3Dot(cXZW, pl.q) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.q);
        cam3Y = camera.getPos().y - map3D.camRef4D.y;
        cam3Yaw = 0; cam3Pitch = 0;
    }

    SetWindowText(hwnd, L"Minecrafx");

    for (auto &input : InputHandler(hwnd))
    {
        // ---- 帧间隔 ----
        clock_t nowFrame = clock();
        double dt = static_cast<double>(nowFrame - lastFrame) / CLOCKS_PER_SEC;
        lastFrame = nowFrame;
        if (dt > 0.1) dt = 0.1;
        if (dt <= 0.0) dt = 0.001;

        // ---- 键盘移动（3D 空间，方向由 4D 相机基向量投影决定） ----
        {
            Plane2D pl = map3D.plane;
            const Vec4 &fwd = camera.getForward(), &rht = camera.getRight();
            double fU = fwd.x * pl.p.x + fwd.z * pl.p.z + fwd.w * pl.p.w;
            double fV = fwd.x * pl.q.x + fwd.z * pl.q.z + fwd.w * pl.q.w;
            double rU = rht.x * pl.p.x + rht.z * pl.p.z + rht.w * pl.p.w;
            double rV = rht.x * pl.q.x + rht.z * pl.q.z + rht.w * pl.q.w;
            // 同步 cam3Yaw
            cam3Yaw = std::atan2(fU, fV);

            double moveU = 0, moveV = 0;
            double speed = MOVE_SPEED * dt;
            if (input.isKeyDown(Key::W)) { moveU += fU * speed; moveV += fV * speed; }
            if (input.isKeyDown(Key::S)) { moveU -= fU * speed; moveV -= fV * speed; }
            if (input.isKeyDown(Key::D)) { moveU += rU * speed; moveV += rV * speed; }
            if (input.isKeyDown(Key::A)) { moveU -= rU * speed; moveV -= rV * speed; }

            // 模式切换 & 跳跃
            if (input.isPressed(Key::Space))
            {
                clock_t now = clock();
                double elapsed = static_cast<double>(now - lastSpacePress) * 1000.0 / CLOCKS_PER_SEC;
                if (elapsed < DOUBLE_TAP_MS && lastSpacePress != 0)
                {
                    flyMode = !flyMode; verticalVel = 0.0; lastSpacePress = 0;
                }
                else
                {
                    lastSpacePress = now;
                    if (!flyMode && onGround) { verticalVel = JUMP_VEL; onGround = false; }
                }
            }

            double moveY = 0;
            if (flyMode)
            {
                if (input.isKeyDown(Key::Space)) moveY = speed;
                if (input.isKeyDown(Key::LShift) || input.isKeyDown(Key::RShift)) moveY = -speed;
            }
            else
            {
                verticalVel -= GRAVITY * dt;
                moveY = verticalVel * dt;
            }

            // ---- 碰撞检测（对 3D 地图 AABB） ----
            double cR = CYLINDER_R, cH = CYLINDER_H;
            auto mapCollide = [&](double u, double v, double y) -> bool
            {
                double uLo = u - cR, uHi = u + cR;
                double vLo = v - cR, vHi = v + cR;
                double yLo = y - cH, yHi = y;
                for (auto &ab : map3D.aabbs)
                {
                    if (uLo < ab.uMax && uHi > ab.uMin &&
                        vLo < ab.vMax && vHi > ab.vMin &&
                        yLo < ab.yMax && yHi > ab.yMin)
                        return true;
                }
                return false;
            };

            double newU = cam3U, newV = cam3V, newY = cam3Y;
            // 水平
            if (!mapCollide(cam3U + moveU, cam3V + moveV, cam3Y))
            {
                newU += moveU; newV += moveV;
            }
            // 垂直
            if (std::abs(moveY) > 1e-12)
            {
                if (!mapCollide(newU, newV, cam3Y + moveY))
                {
                    newY += moveY; if (!flyMode) onGround = false;
                }
                else if (!flyMode && moveY < 0) onGround = true;
            }
            else if (!flyMode && verticalVel <= 0)
            {
                onGround = mapCollide(newU, newV, cam3Y - 0.001);
            }

            cam3U = newU; cam3V = newV; cam3Y = newY;
            if (onGround && verticalVel < 0) verticalVel = 0;

            // ---- 同步 4D 摄像机位置 ----
            {
                Plane2D pl = map3D.plane;
                const Vec4 &ref = map3D.camRef4D;
                Vec4 new4D = ref;
                new4D.x += cam3U * pl.p.x + cam3V * pl.q.x;
                new4D.z += cam3U * pl.p.z + cam3V * pl.q.z;
                new4D.w += cam3U * pl.p.w + cam3V * pl.q.w;
                new4D.y = ref.y + cam3Y;
                Vec4 delta = vec4Sub(new4D, camera.getPos());
                if (vec4LengthSq(delta) > 1e-12) camera.move(delta);
            }
        }

        // ---- 3D 视角旋转（鼠标） ----
        {
            auto [dx, dy] = input.getMouseDelta();
            if (dx != 0) { cam3Yaw += dx * MOUSE_SENSITIVITY; camera.rotateAroundUp(dx * MOUSE_SENSITIVITY); }
            if (dy != 0) { cam3Pitch -= dy * MOUSE_SENSITIVITY; camera.addPitch(-dy * MOUSE_SENSITIVITY); }
        }

        // ---- Q/E/滚轮：重建地图 ----
        {
            double inputDesire = 0.0;
            int wheel = input.getMouseWheel();
            if (wheel != 0) inputDesire += (wheel / (double) WHEEL_DELTA) * SLICE_STEP;
            if (input.isKeyDown(Key::E)) inputDesire += SLICE_STEP;
            if (input.isKeyDown(Key::Q)) inputDesire -= SLICE_STEP;
            sliceVelocity += (inputDesire - sliceVelocity) * SLICE_SMOOTH;

            if (std::abs(sliceVelocity) > 1e-10)
            {
                camera.rotateSlice(sliceVelocity);
                // 重建 3D 地图
                map3D = generateMap3D(world, camera, 0.5,
                    [](int bx, int by, int bz, int bw) -> COLORREF
                {
                    return blockColor(bx, by, bz, bw);
                });
                // 重新计算 3D 位置
                Plane2D pl = camera.getViewPlane();
                Vec3 cXZW = Vec3::fromVec4(camera.getPos());
                cam3U = vec3Dot(cXZW, pl.p) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.p);
                cam3V = vec3Dot(cXZW, pl.q) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.q);
                cam3Y = camera.getPos().y - map3D.camRef4D.y;
            }
            else sliceVelocity = 0;
        }


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
