#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <functional>
#include <iostream>
#include <windows.h>

// ============================================================================
// 构造 / 析构
// ============================================================================

Renderer::Renderer(int screenWidth, int screenHeight)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_blockHalf(0.5 / 16.0)
    , m_frameCount(0)
    , m_hBmp(nullptr)
    , m_memDC(nullptr)
    , m_oldBmp(nullptr)
    , m_dibReady(false)
    , m_fpsFrames(0)
    , m_fpsTime(0)
    , m_fps(0)
    , m_diagTotal(0)
    , m_diagSlice(0)
    , m_diagOccl(0)
    , m_diagGeom(0)
    , m_diagFaces(0)
    , m_texLoaded(false)
{
    m_zbuf.resize(m_screenWidth * m_screenHeight);
    memset(m_tex, 0, sizeof(m_tex));

    // 预创建 DIB
    HDC hdc = GetImageHDC();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_screenWidth;
    bmi.bmiHeader.biHeight = -m_screenHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    DWORD *bits = nullptr;
    m_hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
        reinterpret_cast<void **>(&bits), nullptr, 0);
    if (m_hBmp && bits)
    {
        m_memDC = CreateCompatibleDC(hdc);
        m_oldBmp = (HBITMAP) SelectObject(m_memDC, m_hBmp);
        m_pBits = bits;
        m_dibReady = true;
    }
}

Renderer::~Renderer()
{
    if (m_dibReady)
    {
        SelectObject(m_memDC, m_oldBmp);
        DeleteDC(m_memDC);
        DeleteObject(m_hBmp);
    }
}

// ============================================================================
// 纹理加载
// ============================================================================

static COLORREF blockColor(int x, int y, int z, int w)
{
    unsigned int h = static_cast<unsigned int>(
        x * 73856093 + y * 19349663 + z * 83492791 + w * 39916801);
    h = (h ^ (h >> 13)) * 0x9e3779b9;
    int r = 60 + (h & 0xFF) % 156;
    int g = 60 + ((h >> 8) & 0xFF) % 156;
    int b = 60 + ((h >> 16) & 0xFF) % 156;
    return RGB(r, g, b);
}

COLORREF Renderer::getBlockColor(int x, int y, int z, int w) const
{
    if (m_texLoaded)
    {
        int tx = (x % 16 + 16) % 16;
        int ty = (y % 16 + 16) % 16;
        int tz = (z % 16 + 16) % 16;
        int tw = (w % 16 + 16) % 16;
        return m_tex[tx][ty][tz][tw];
    }
    return blockColor(x, y, z, w);
}

void Renderer::loadTextures(const wchar_t *basePath)
{
    wchar_t path[512];
    for (int x = 0; x < 16; ++x)
    {
        for (int z = 0; z < 16; ++z)
        {
            swprintf(path, 512, L"%ls/x%02d/z%02d.png", basePath, x, z);
            IMAGE img;
            loadimage(&img, path);
            DWORD *buf = GetImageBuffer(&img);
            if (buf && img.getwidth() > 0)
            {
                int w = img.getwidth(), h = img.getheight();
                for (int y = 0; y < 16 && y < h; ++y)
                    for (int ww = 0; ww < 16 && ww < w; ++ww)
                        m_tex[x][15 - y][z][ww] = buf[y * w + ww];
            }
        }
    }
    m_texLoaded = true;
}

// ============================================================================
// 缓冲管理
// ============================================================================

void Renderer::resetBuffers()
{
    std::fill(m_zbuf.begin(), m_zbuf.end(), 1e30);
}

// ============================================================================
// 主渲染入口
// ============================================================================

void Renderer::renderWorld(const World &world, const Camera4D &cam)
{
    if (!m_dibReady) return;

    ++m_frameCount;

    // 1. 清空帧缓冲
    resetBuffers();

    DWORD bg = 0x001E0A0A;
    int total = m_screenWidth * m_screenHeight;
    DWORD *bits = m_pBits;
    for (int i = 0; i < total; ++i) bits[i] = bg;

    // 2. 获取观察平面
    Plane2D plane = cam.getViewPlane();

    // 3. 诊断计数初始化
    m_diagTotal = static_cast<int>(world.getAllBlocks().size());
    m_diagSlice = 0;
    m_diagOccl = 0;
    m_diagGeom = 0;
    m_diagFaces = 0;

    // 4. 收集可见方块（含统计）
    std::vector<IVec4> visibleBlocks = collectVisibleBlocks(world, cam, plane);
    m_diagSlice = static_cast<int>(visibleBlocks.size());
    m_diagOccl = m_diagSlice;

    if (!visibleBlocks.empty())
    {
        // 5. 设置 3D 相机（提前，用于视锥体裁剪）
        Camera3D cam3d;
        {
            Vec3 camXZW = Vec3::fromVec4(cam.getPos());
            double camU = vec3Dot(camXZW, plane.p);
            double camV = vec3Dot(camXZW, plane.q);
            double pitch = cam.getPitch();
            double sP = std::sin(pitch), cP = std::cos(pitch);
            cam3d.posU = camU;
            cam3d.posV = camV;
            cam3d.posY = cam.getPos().y;
            cam3d.dirU = 0.0;
            cam3d.dirV = cP;
            cam3d.dirY = sP;
        }

        // 6. 4D→3D：方块 → 三角形（含视锥体裁剪）
        {
            std::vector<Tri3D> allTris;
            allTris.reserve(visibleBlocks.size() * 12);

            double half = m_blockHalf, sp = half * 2.0;
            const Vec4 &camPos = cam.getPos();

            // 预计算视锥体裁剪用的 camera right/up
            double rU = cam3d.dirV, rV = -cam3d.dirU;
            double rLen = std::sqrt(rU * rU + rV * rV);
            rU /= rLen; rV /= rLen;
            double upU = rV * cam3d.dirY, upV = -rU * cam3d.dirY, upY = rU * cam3d.dirV - rV * cam3d.dirU;

            for (const auto &blk : visibleBlocks)
            {
                // 方块中心在 3D 空间的近似坐标
                double bu = vec3Dot(Vec3(blk.x * sp - camPos.x, blk.z * sp - camPos.z, blk.w * sp - camPos.w), plane.p);
                double bv = vec3Dot(Vec3(blk.x * sp - camPos.x, blk.z * sp - camPos.z, blk.w * sp - camPos.w), plane.q);
                double by = blk.y * sp - camPos.y;

                // 变换到相机空间
                double dU = bu - cam3d.posU;
                double dV = bv - cam3d.posV;
                double dY = by - cam3d.posY;
                double camZ = cam3d.dirU * dU + cam3d.dirV * dV + cam3d.dirY * dY;

                // 在相机后方或太远则跳过
                if (camZ < cam3d.nearPlane || camZ > cam3d.farPlane) continue;

                // 视锥体水平/垂直检查（加 margin）
                double margin = half * 3.0;
                double camX = rU * dU + rV * dV;
                double camY = upU * dU + upV * dV + upY * dY;
                double halfH = std::tan(cam3d.fov * 0.5) * camZ;
                double halfW = halfH * m_screenWidth / m_screenHeight;
                if (camX < -halfW - margin || camX > halfW + margin) continue;
                if (camY < -halfH - margin || camY > halfH + margin) continue;

                COLORREF col = getBlockColor(blk.x, blk.y, blk.z, blk.w);
                size_t before = allTris.size();
                blockToTriangles(blk.x, blk.y, blk.z, blk.w, cam, plane, col, allTris);
                if (allTris.size() > before) ++m_diagGeom;
            }

            m_diagFaces = static_cast<int>(allTris.size());

            if (!allTris.empty())
            {
                // 7. 光栅化
                rasterizeTriangles(allTris, cam3d);
            }
        }
    }

    // 8. 输出 DIB 到屏幕
    HDC hdc = GetImageHDC();
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, m_memDC, 0, 0, SRCCOPY);

    // 9. 绘制 HUD 和准星
    drawHUD(cam);
    drawCrosshair();

    // 10. FPS 统计
    ++m_fpsFrames;
    clock_t now = clock();
    double elapsed = static_cast<double>(now - m_fpsTime) / CLOCKS_PER_SEC;
    if (elapsed >= 1.0)
    {
        m_fps = static_cast<int>(m_fpsFrames / elapsed);
        m_fpsFrames = 0;
        m_fpsTime = now;
    }
}

// ============================================================================
// 准星
// ============================================================================

void Renderer::drawCrosshair()
{
    int cx = m_screenWidth / 2;
    int cy = m_screenHeight / 2;
    int gap = 6;
    int len = 10;

    setlinecolor(RGB(255, 255, 255));
    line(cx - len, cy, cx - gap, cy);
    line(cx + gap, cy, cx + len, cy);
    line(cx, cy - len, cx, cy - gap);
    line(cx, cy + gap, cx, cy + len);
}

// ============================================================================
// HUD
// ============================================================================

void Renderer::drawHUD(const Camera4D &cam)
{
    const Vec4 &pos = cam.getPos();
    const Vec4 &r = cam.getRight();
    const Vec4 &f = cam.getForward();
    const Vec4 &o = cam.getOver();

    wchar_t buf[256];
    HDC hdc = GetImageHDC();
    SetBkMode(hdc, TRANSPARENT);

    // ========================================
    // 左上角：XZW 三维坐标系可视化
    // ========================================
    const int vpX = 10, vpY = 10, vpW = 150, vpH = 140;
    const int ox = vpX + vpW / 2, oy = vpY + vpH / 2 + 5;
    const double vs = 45.0;

    // 黑底
    setfillcolor(BLACK);
    solidrectangle(vpX, vpY, vpX + vpW, vpY + vpH);

    auto proj3 = [&](double vx, double vz, double vw) -> POINT
    {
        const double k = 0.35355339;
        return {
            static_cast<int>(ox + vz * vs - vx * vs * k),
            static_cast<int>(oy - vw * vs + vx * vs * k)
        };
    };

    POINT oPt = proj3(0, 0, 0);

    // ---- n 的垂直平面（半透明粉色四边形） ----
    Vec4 n3 = Vec4(o.x, 0.0, o.z, o.w);
    double nLen = vec4Length(n3);
    if (nLen > 1e-9) { n3 = vec4Scale(n3, 1.0 / nLen); }

    Vec4 u3, v3;
    if (std::abs(n3.x) < 0.9)      u3 = Vec4(1.0, 0.0, 0.0, 0.0);
    else if (std::abs(n3.z) < 0.9) u3 = Vec4(0.0, 0.0, 1.0, 0.0);
    else                           u3 = Vec4(0.0, 0.0, 0.0, 1.0);
    double dotUN = u3.x * n3.x + u3.z * n3.z + u3.w * n3.w;
    u3 = Vec4(u3.x - dotUN * n3.x, 0.0, u3.z - dotUN * n3.z, u3.w - dotUN * n3.w);
    double uLen = std::sqrt(u3.x * u3.x + u3.z * u3.z + u3.w * u3.w);
    if (uLen > 1e-9) { u3.x /= uLen; u3.z /= uLen; u3.w /= uLen; }
    v3.x = n3.z * u3.w - n3.w * u3.z;
    v3.z = n3.w * u3.x - n3.x * u3.w;
    v3.w = n3.x * u3.z - n3.z * u3.x;

    const double pHalf = 1.1;
    POINT pCorners[4] = {
        proj3(u3.x * pHalf + v3.x * pHalf,  u3.z * pHalf + v3.z * pHalf,  u3.w * pHalf + v3.w * pHalf),
        proj3(u3.x * pHalf - v3.x * pHalf,  u3.z * pHalf - v3.z * pHalf,  u3.w * pHalf - v3.w * pHalf),
        proj3(-u3.x * pHalf - v3.x * pHalf, -u3.z * pHalf - v3.z * pHalf, -u3.w * pHalf - v3.w * pHalf),
        proj3(-u3.x * pHalf + v3.x * pHalf, -u3.z * pHalf + v3.z * pHalf, -u3.w * pHalf + v3.w * pHalf)
    };
    setfillcolor(RGB(80, 60, 90));
    setlinecolor(RGB(150, 100, 180));
    fillpolygon(pCorners, 4);

    // ---- 坐标轴 ----
    auto drawArrow2 = [](POINT from, POINT to, COLORREF clr)
    {
        setlinecolor(clr);
        line(from.x, from.y, to.x, to.y);
    };

    POINT xPt = proj3(1.2, 0, 0);
    POINT zPt = proj3(0, 1.2, 0);
    POINT wPt = proj3(0, 0, 1.2);
    drawArrow2(oPt, xPt, RGB(255, 100, 100));
    drawArrow2(oPt, zPt, RGB(100, 255, 100));
    drawArrow2(oPt, wPt, RGB(100, 150, 255));

    settextcolor(RGB(255, 100, 100)); TextOutW(hdc, xPt.x - 20, xPt.y + 2, L"X", 1);
    settextcolor(RGB(100, 255, 100)); TextOutW(hdc, zPt.x + 2, zPt.y - 8, L"Z", 1);
    settextcolor(RGB(100, 150, 255)); TextOutW(hdc, wPt.x - 8, wPt.y - 18, L"W", 1);

    // i（forward，黄色）
    Vec4 i3 = Vec4(f.x, 0.0, f.z, f.w);
    double iLen = vec4Length(i3);
    if (iLen > 1e-9) { i3 = vec4Scale(i3, 1.0 / iLen); }
    POINT iP = proj3(i3.x, i3.z, i3.w);
    drawArrow2(oPt, iP, RGB(255, 220, 50));
    settextcolor(RGB(255, 220, 50)); TextOutW(hdc, iP.x + 3, iP.y - 10, L"i", 1);

    // j（right，青色）
    Vec4 j3 = Vec4(r.x, 0.0, r.z, r.w);
    double jLen = vec4Length(j3);
    if (jLen > 1e-9) { j3 = vec4Scale(j3, 1.0 / jLen); }
    POINT jP = proj3(j3.x, j3.z, j3.w);
    drawArrow2(oPt, jP, RGB(50, 220, 220));
    settextcolor(RGB(50, 220, 220)); TextOutW(hdc, jP.x + 3, jP.y - 10, L"j", 1);

    // n（over，粉色法向）
    n3 = Vec4(o.x, 0.0, o.z, o.w);
    nLen = vec4Length(n3);
    if (nLen > 1e-9) { n3 = vec4Scale(n3, 1.0 / nLen); }
    POINT nP = proj3(n3.x, n3.z, n3.w);
    drawArrow2(oPt, nP, RGB(255, 120, 255));
    settextcolor(RGB(255, 120, 255)); TextOutW(hdc, nP.x + 3, nP.y - 10, L"n", 1);

    // ========================================
    // FPS + 诊断（右上角）
    // ========================================
    settextcolor(RGB(255, 255, 255));
    swprintf(buf, 256, L"FPS: %d", m_fps);
    TextOutW(hdc, m_screenWidth - 100, 10, buf, (int) wcslen(buf));

    settextcolor(RGB(255, 255, 100));
    swprintf(buf, 256, L"方块总数: %d", m_diagTotal);
    TextOutW(hdc, m_screenWidth - 200, 30, buf, (int) wcslen(buf));
    swprintf(buf, 256, L"切片通过: %d", m_diagSlice);
    TextOutW(hdc, m_screenWidth - 200, 48, buf, (int) wcslen(buf));
    swprintf(buf, 256, L"遮挡通过: %d", m_diagOccl);
    TextOutW(hdc, m_screenWidth - 200, 66, buf, (int) wcslen(buf));
    swprintf(buf, 256, L"几何生成: %d", m_diagGeom);
    TextOutW(hdc, m_screenWidth - 200, 84, buf, (int) wcslen(buf));
    swprintf(buf, 256, L"渲染面数: %d", m_diagFaces);
    TextOutW(hdc, m_screenWidth - 200, 102, buf, (int) wcslen(buf));
    settextcolor(RGB(255, 255, 255));

    // ========================================
    // 坐标信息（左侧，坐标系下方）
    // ========================================
    int infoY = vpY + vpH + 5;
    swprintf(buf, 256, L"%.1f,  %.1f,  %.1f,  %.1f",
        pos.x, pos.y, pos.z, pos.w);
    TextOutW(hdc, 10, infoY, buf, (int) wcslen(buf));
    infoY += 18;

    const double rad2deg = 180.0 / 3.1415926535;
    swprintf(buf, 256, L"俯仰角: %+.1f°", cam.getPitch() * rad2deg);
    TextOutW(hdc, 10, infoY, buf, (int) wcslen(buf));
}

// ============================================================================
// 方块收集
// ============================================================================

static bool blockIntersectsPlane(int bx, int /*by*/, int bz, int bw,
    const Vec4 &camPos, const Plane2D &plane, double half, double sp)
{
    double x0 = bx * sp - camPos.x - half;
    double x1 = bx * sp - camPos.x + half;
    double z0 = bz * sp - camPos.z - half;
    double z1 = bz * sp - camPos.z + half;
    double w0 = bw * sp - camPos.w - half;
    double w1 = bw * sp - camPos.w + half;

    double minDot = 0.0, maxDot = 0.0;
    double nx = plane.n.x, nz = plane.n.z, nw = plane.n.w;

    if (nx > 0) { minDot += nx * x0; maxDot += nx * x1; }
    else { minDot += nx * x1; maxDot += nx * x0; }
    if (nz > 0) { minDot += nz * z0; maxDot += nz * z1; }
    else { minDot += nz * z1; maxDot += nz * z0; }
    if (nw > 0) { minDot += nw * w0; maxDot += nw * w1; }
    else { minDot += nw * w1; maxDot += nw * w0; }

    return minDot <= 0.0 && maxDot >= 0.0;
}

std::vector<IVec4> Renderer::collectVisibleBlocks(const World &world,
    const Camera4D &cam, const Plane2D &plane)
{
    std::vector<IVec4> result;

    const Vec4 &camPos = cam.getPos();
    double sp = m_blockHalf * 2.0;

    // ---- 世界独立方块 ----
    const auto &blocks = world.getAllBlocks();
    for (const auto &pair : blocks)
    {
        int bx = pair.first.x, by = pair.first.y;
        int bz = pair.first.z, bw = pair.first.w;

        // 遮挡剔除：8 个方向都有相邻方块则不可见
        if (world.get(IVec4(bx + 1, by, bz, bw)) &&
            world.get(IVec4(bx - 1, by, bz, bw)) &&
            world.get(IVec4(bx, by + 1, bz, bw)) &&
            world.get(IVec4(bx, by - 1, bz, bw)) &&
            world.get(IVec4(bx, by, bz + 1, bw)) &&
            world.get(IVec4(bx, by, bz - 1, bw)) &&
            world.get(IVec4(bx, by, bz, bw + 1)) &&
            world.get(IVec4(bx, by, bz, bw - 1)))
            continue;

        if (blockIntersectsPlane(bx, by, bz, bw, camPos, plane, m_blockHalf, sp))
            result.push_back(IVec4(bx, by, bz, bw));
    }

    // ---- 超方块：十六分法递归遍历 ----
    for (const auto &sb : m_superBlocks)
        sb.collectVisible(camPos, plane, m_blockHalf, result);

    return result;
}

// ============================================================================
// 4D→3D：方块 → 三角形
// ============================================================================

void Renderer::blockToTriangles(int bx, int by, int bz, int bw,
    const Camera4D &cam, const Plane2D &plane,
    COLORREF color, std::vector<Tri3D> &outTris)
{
    const Vec4 &camPos = cam.getPos();
    double half = m_blockHalf;
    double sp = half * 2.0;

    double x0 = bx * sp - camPos.x - half;
    double x1 = bx * sp - camPos.x + half;
    double z0 = bz * sp - camPos.z - half;
    double z1 = bz * sp - camPos.z + half;
    double w0 = bw * sp - camPos.w - half;
    double w1 = bw * sp - camPos.w + half;

    // 相机空间平面（过原点，offset=0）
    Plane2D camPlane = plane;
    camPlane.offset = 0.0;

    PolyOnPlane poly = intersectCubePlane(x0, x1, z0, z1, w0, w1, camPlane);
    if (!poly.valid()) return;

    double yLow = by * sp - camPos.y - half;
    double yHigh = by * sp - camPos.y + half;

    // 相机视线 Y 分量，用于顶/底面背向剔除
    double dirY = std::sin(cam.getPitch());

    int n = poly.n;
    const auto &pu = poly.u;
    const auto &pv = poly.v;

    // 顶面和底面：扇形三角剖分（各 n-2 个三角形）
    for (int i = 1; i < n - 1; ++i)
    {
        // 顶面（法线 +Y）：视线朝上时背向
        if (dirY <= 0.0)
        {
            Tri3D t;
            t.u[0] = pu[0];     t.v[0] = pv[0];     t.y[0] = yHigh;
            t.u[1] = pu[i];     t.v[1] = pv[i];     t.y[1] = yHigh;
            t.u[2] = pu[i + 1]; t.v[2] = pv[i + 1]; t.y[2] = yHigh;
            t.color = color;
            t.depth = yHigh;
            outTris.push_back(t);
        }

        // 底面（法线 -Y）：视线朝下时背向
        if (dirY >= 0.0)
        {
            Tri3D b;
            b.u[0] = pu[0];     b.v[0] = pv[0];     b.y[0] = yLow;
            b.u[2] = pu[i];     b.v[2] = pv[i];     b.y[2] = yLow;
            b.u[1] = pu[i + 1]; b.v[1] = pv[i + 1]; b.y[1] = yLow;
            b.color = color;
            b.depth = yLow;
            outTris.push_back(b);
        }
    }

    // 侧面：n 条边，每条边 2 个三角形
    for (int i = 0; i < n; ++i)
    {
        int j = (i + 1) % n;

        Tri3D t1;
        t1.u[0] = pu[i]; t1.v[0] = pv[i]; t1.y[0] = yLow;
        t1.u[1] = pu[j]; t1.v[1] = pv[j]; t1.y[1] = yLow;
        t1.u[2] = pu[i]; t1.v[2] = pv[i]; t1.y[2] = yHigh;
        t1.color = color;
        t1.depth = (yLow + yHigh) * 0.5;
        outTris.push_back(t1);

        Tri3D t2;
        t2.u[0] = pu[j]; t2.v[0] = pv[j]; t2.y[0] = yLow;
        t2.u[1] = pu[j]; t2.v[1] = pv[j]; t2.y[1] = yHigh;
        t2.u[2] = pu[i]; t2.v[2] = pv[i]; t2.y[2] = yHigh;
        t2.color = color;
        t2.depth = (yLow + yHigh) * 0.5;
        outTris.push_back(t2);
    }
}

// ============================================================================
// 3D→2D 光栅化
// ============================================================================

void Renderer::rasterizeTriangles(const std::vector<Tri3D> &tris,
    const Camera3D &cam3d)
{
    for (const auto &tri : tris)
        rasterizeTriangle(tri, cam3d);
}

void Renderer::rasterizeTriangle(const Tri3D &tri, const Camera3D &cam3d)
{
    int sx[3], sy[3];
    double sz[3];
    bool valid[3];

    for (int i = 0; i < 3; ++i)
    {
        valid[i] = project3D(tri.u[i], tri.v[i], tri.y[i],
            cam3d, m_screenWidth, m_screenHeight,
            sx[i], sy[i], sz[i]);
    }

    if (!valid[0] && !valid[1] && !valid[2]) return;

    // 按 y 排序
    int idx[3] = { 0, 1, 2 };
    if (sy[idx[0]] > sy[idx[1]]) std::swap(idx[0], idx[1]);
    if (sy[idx[1]] > sy[idx[2]]) std::swap(idx[1], idx[2]);
    if (sy[idx[0]] > sy[idx[1]]) std::swap(idx[0], idx[1]);

    int y0 = sy[idx[0]], y1 = sy[idx[1]], y2 = sy[idx[2]];
    int x0 = sx[idx[0]], x1 = sx[idx[1]], x2 = sx[idx[2]];
    double z0 = sz[idx[0]], z1 = sz[idx[1]], z2 = sz[idx[2]];

    if (y2 < 0 || y0 >= m_screenHeight) return;
    if (y0 == y2) return;

    // 上半部分：y0 → y1
    if (y1 > y0)
    {
        double invDyTop = 1.0 / static_cast<double>(y1 - y0);
        double invDyFull = 1.0 / static_cast<double>(y2 - y0);
        double dxL = static_cast<double>(x1 - x0) * invDyTop;
        double dxR = static_cast<double>(x2 - x0) * invDyFull;
        double dzL = (z1 - z0) * invDyTop;
        double dzR = (z2 - z0) * invDyFull;

        int yStart = std::max(y0, 0);
        int yEnd = std::min(y1, m_screenHeight);

        for (int y = yStart; y < yEnd; ++y)
        {
            double t = static_cast<double>(y - y0);
            int xL = static_cast<int>(x0 + dxL * t);
            int xR = static_cast<int>(x0 + dxR * t);
            double zL = z0 + dzL * t;
            double zR = z0 + dzR * t;
            if (xL > xR) { std::swap(xL, xR); std::swap(zL, zR); }
            drawScanline(y, xL, xR, zL, zR, tri.color);
        }
    }

    // 下半部分：y1 → y2
    if (y2 > y1)
    {
        double invDyBot = 1.0 / static_cast<double>(y2 - y1);
        double invDyFull = 1.0 / static_cast<double>(y2 - y0);
        double dxL = static_cast<double>(x2 - x0) * invDyFull;
        double dxR = static_cast<double>(x2 - x1) * invDyBot;
        double dzL = (z2 - z0) * invDyFull;
        double dzR = (z2 - z1) * invDyBot;

        int yStart = std::max(y1, 0);
        int yEnd = std::min(y2, m_screenHeight);

        for (int y = yStart; y < yEnd; ++y)
        {
            double tFull = static_cast<double>(y - y0);
            double tBot = static_cast<double>(y - y1);
            int xL = static_cast<int>(x0 + dxL * tFull);
            int xR = static_cast<int>(x1 + dxR * tBot);
            double zL = z0 + dzL * tFull;
            double zR = z1 + dzR * tBot;
            if (xL > xR) { std::swap(xL, xR); std::swap(zL, zR); }
            drawScanline(y, xL, xR, zL, zR, tri.color);
        }
    }
}

void Renderer::drawScanline(int y, int x0, int x1, double z0, double z1,
    COLORREF color)
{
    if (y < 0 || y >= m_screenHeight) return;

    if (x0 < 0) x0 = 0;
    if (x1 > m_screenWidth) x1 = m_screenWidth;
    if (x0 >= x1) return;

    int rowBase = y * m_screenWidth;
    double invDx = 1.0 / static_cast<double>(x1 - x0);
    double dz = (z1 - z0) * invDx;

    for (int x = x0; x < x1; ++x)
    {
        double t = static_cast<double>(x - x0);
        double z = z0 + dz * t;

        int idx = rowBase + x;
        if (z < m_zbuf[idx])
        {
            m_zbuf[idx] = z;
            m_pBits[idx] = color;
        }
    }
}
