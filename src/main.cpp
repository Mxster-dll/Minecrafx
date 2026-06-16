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
#include <cstdlib>

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

    // ---- 加载方块贴图 ----
    renderer.loadBlockTextures();

    // ---- 4D 丘陵地貌 ----
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
                int ht = terrainHeight(x, z, w);
                if (ht < 1) ht = 1;
                for (int y = 0; y < ht; ++y)
                {
                    int type = BLOCK_DIRT;
                    if (y == ht - 1) type = BLOCK_GRASS;  // 表面草方块
                    world.set(IVec4(x, y, z, w), type);
                }
            }

    // ---- 4D 树木 ----
    srand(42);  // 固定种子保证可复现
    for (int i = 0; i < 40; ++i)
    {
        int tx = rand() % (MX - 2) + 1;
        int tz = rand() % (MZ - 2) + 1;
        int tw = rand() % (MW - 2) + 1;
        int groundY = terrainHeight(tx, tz, tw);
        if (groundY < 2) continue;

        // 树干：3~5 格高
        int trunkH = 3 + rand() % 3;
        for (int ty = groundY; ty < groundY + trunkH; ++ty)
            world.set(IVec4(tx, ty, tz, tw), BLOCK_LOG);

        // 树叶：在 x/z/w 三个方向围绕树冠展开
        int leafBase = groundY + trunkH - 1;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz)
                for (int dw = -1; dw <= 1; ++dw)
                {
                    int lx = tx + dx, lz = tz + dz, lw = tw + dw;
                    if (lx < 0 || lx >= MX || lz < 0 || lz >= MZ || lw < 0 || lw >= MW) continue;
                    // 树干位置不替换
                    if (dx == 0 && dz == 0 && dw == 0) continue;
                    // 角落稀疏化
                    int corner = (dx != 0) + (dz != 0) + (dw != 0);
                    if (corner >= 2 && (rand() % 3) != 0) continue;
                    for (int ly = leafBase; ly <= leafBase + 2; ++ly)
                    {
                        if (ly >= 20) continue;
                        world.set(IVec4(lx, ly, lz, lw), BLOCK_LEAVES);
                    }
                }
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
    int interactCooldown = 0;  // 放置/摧毁冷却帧数

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

            // ---- 碰撞检测（棱柱多边形 + 法向量） ----
            double cR = CYLINDER_R, cH = CYLINDER_H;
            auto collideNormal = [&](double u, double v, double y, double &nU, double &nV) -> bool
            {
                nU = nV = 0; double bestDist = cR;
                for (size_t pi = 0; pi < map3D.prisms.size(); ++pi)
                {
                    auto &ab = map3D.aabbs[pi];
                    if (u - cR > ab.uMax || u + cR < ab.uMin ||
                        v - cR > ab.vMax || v + cR < ab.vMin ||
                        y - cH > ab.yMax || y < ab.yMin) continue;
                    auto &pr = map3D.prisms[pi];
                    int n = (int) pr.u.size();
                    // 点在凸多边形内？（修复地板穿透）
                    bool inside = true;
                    for (int i = 0; i < n && inside; ++i)
                    {
                        int j = (i + 1) % n;
                        double eu = pr.u[j] - pr.u[i], ev = pr.v[j] - pr.v[i];
                        if (eu * (v - pr.v[i]) - ev * (u - pr.u[i]) < -1e-9) inside = false;
                    }
                    if (inside) return true;
                    // 圆 vs 凸多边形边
                    for (int i = 0; i < n; ++i)
                    {
                        int j = (i + 1) % n;
                        double eu = pr.u[j] - pr.u[i], ev = pr.v[j] - pr.v[i];
                        double len2 = eu * eu + ev * ev;
                        double t = ((u - pr.u[i]) * eu + (v - pr.v[i]) * ev) / len2;
                        if (t < 0) t = 0; else if (t > 1) t = 1;
                        double cu = pr.u[i] + t * eu, cv = pr.v[i] + t * ev;
                        double du = u - cu, dv = v - cv;
                        double dist = std::sqrt(du * du + dv * dv);
                        if (dist < bestDist) { bestDist = dist; nU = du / dist; nV = dv / dist; }
                    }
                }
                return bestDist < cR;
            };
            auto mapCollide = [&](double u, double v, double y) -> bool
            {
                double nU, nV; return collideNormal(u, v, y, nU, nV);
            };

            double newU = cam3U, newV = cam3V, newY = cam3Y;
            // 水平：法向投影滑动
            {
                double nU, nV;
                if (!collideNormal(cam3U + moveU, cam3V + moveV, cam3Y, nU, nV))
                {
                    newU += moveU; newV += moveV;
                }
                else
                {
                    // 投影到面：去掉法向分量
                    double dot = moveU * nU + moveV * nV;
                    if (dot > 0) { moveU -= dot * nU; moveV -= dot * nV; }
                    if (!mapCollide(cam3U + moveU, cam3V + moveV, cam3Y))
                    {
                        newU += moveU; newV += moveV;
                    }
                }
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


        // 鼠标按键行为（3D 视线射线，带冷却防止连点）
        {
            IVec4 hitPos, prevPos;
            bool changed = false;
            if (interactCooldown > 0) --interactCooldown;
            else
            {
                Plane2D pl = map3D.plane;
                double du = std::sin(cam3Yaw) * std::cos(cam3Pitch);
                double dv = std::cos(cam3Yaw) * std::cos(cam3Pitch);
                double dys = std::sin(cam3Pitch);
                Vec4 rayDir(du * pl.p.x + dv * pl.q.x, dys, du * pl.p.z + dv * pl.q.z, du * pl.p.w + dv * pl.q.w);
                double rLen = vec4Length(rayDir); if (rLen > 1e-9) rayDir = vec4Scale(rayDir, 1.0 / rLen);
                auto raycast3D = [&](IVec4 &hit, IVec4 &prev) -> bool
                {
                    Vec4 pos = camera.getPos();
                    IVec4 pg((int) std::round(pos.x), (int) std::round(pos.y), (int) std::round(pos.z), (int) std::round(pos.w));
                    for (double t = 0.2; t <= 8.0; t += 0.2)
                    {
                        Vec4 s = vec4Add(pos, vec4Scale(rayDir, t));
                        IVec4 g((int) std::round(s.x), (int) std::round(s.y), (int) std::round(s.z), (int) std::round(s.w));
                        if (g.x == pg.x && g.y == pg.y && g.z == pg.z && g.w == pg.w)continue;
                        if (world.get(g)) { hit = g; prev = pg; return true; } pg = g;
                    }
                    return false;
                };
                if (input.getMouseClick(0)) { if (raycast3D(hitPos, prevPos)) { world.set(hitPos, 0); changed = true; interactCooldown = 15; } }
                if (input.getMouseClick(1))
                {
                    if (raycast3D(hitPos, prevPos))
                    {
                        // 临时放置，检查是否会与摄像机碰撞
                        world.set(prevPos, 1);
                        Map3D testMap = generateMap3D(world, camera, 0.5,
                            [](int bx, int by, int bz, int bw)->COLORREF { return blockColor(bx, by, bz, bw); });
                        Plane2D tpl = testMap.plane;
                        Vec3 tXZW = Vec3::fromVec4(camera.getPos());
                        double tU = vec3Dot(tXZW, tpl.p) - vec3Dot(Vec3::fromVec4(testMap.camRef4D), tpl.p);
                        double tV = vec3Dot(tXZW, tpl.q) - vec3Dot(Vec3::fromVec4(testMap.camRef4D), tpl.q);
                        double tY = camera.getPos().y - testMap.camRef4D.y;
                        double cR = CYLINDER_R, cH = CYLINDER_H;
                        bool wouldCollide = false;
                        for (size_t pi = 0; pi < testMap.prisms.size() && !wouldCollide; ++pi)
                        {
                            auto &ab = testMap.aabbs[pi];
                            if (tU - cR > ab.uMax || tU + cR < ab.uMin ||
                                tV - cR > ab.vMax || tV + cR < ab.vMin ||
                                tY - cH > ab.yMax || tY < ab.yMin) continue;
                            auto &pr = testMap.prisms[pi];
                            int pn = (int) pr.u.size();
                            bool inside = true;
                            for (int i = 0; i < pn && inside; ++i)
                            {
                                int j = (i + 1) % pn;
                                double eu = pr.u[j] - pr.u[i], ev = pr.v[j] - pr.v[i];
                                if (eu * (tV - pr.v[i]) - ev * (tU - pr.u[i]) < -1e-9) inside = false;
                            }
                            if (inside) { wouldCollide = true; break; }
                            for (int i = 0; i < pn; ++i)
                            {
                                int j = (i + 1) % pn;
                                double eu = pr.u[j] - pr.u[i], ev = pr.v[j] - pr.v[i];
                                double len2 = eu * eu + ev * ev;
                                double t = ((tU - pr.u[i]) * eu + (tV - pr.v[i]) * ev) / len2;
                                if (t < 0)t = 0; else if (t > 1)t = 1;
                                double du = tU - (pr.u[i] + t * eu), dv = tV - (pr.v[i] + t * ev);
                                if (du * du + dv * dv < cR * cR) { wouldCollide = true; break; }
                            }
                        }
                        if (wouldCollide)
                        {
                            world.set(prevPos, 0);  // 撤销放置
                        }
                        else
                        {
                            changed = true; interactCooldown = 15;
                        }
                    }
                }
            }
            if (changed)
            {
                map3D = generateMap3D(world, camera, 0.5,
                    [](int bx, int by, int bz, int bw)->COLORREF { return blockColor(bx, by, bz, bw); });
                Plane2D pl = camera.getViewPlane();
                Vec3 cXZW = Vec3::fromVec4(camera.getPos());
                cam3U = vec3Dot(cXZW, pl.p) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.p);
                cam3V = vec3Dot(cXZW, pl.q) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.q);
                cam3Y = camera.getPos().y - map3D.camRef4D.y;
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
