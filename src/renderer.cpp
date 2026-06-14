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

Renderer::Renderer(int screenWidth, int screenHeight, double scale)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_scale(scale)
    , m_offsetX(screenWidth / 2.0)
    , m_offsetY(screenHeight / 2.0)
    , m_blockHalf(0.5)
    , m_frameCount(0)
    , m_hBmp(nullptr)
    , m_memDC(nullptr)
    , m_oldBmp(nullptr)
    , m_dibReady(false)
    , m_fpsFrames(0)
    , m_fpsTime(0)
    , m_fps(0)
    , m_diagBlocks(0)
    , m_diagVisible(0)
    , m_diagTriangles(0)
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
    // 始终使用随机着色
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

    // 3. 收集可见方块
    m_diagBlocks = 0;
    m_diagVisible = 0;
    m_diagTriangles = 0;

    std::vector<IVec4> visibleBlocks = collectVisibleBlocks(world, cam, plane);
    m_diagVisible = static_cast<int>(visibleBlocks.size());

    if (visibleBlocks.empty()) goto blit;

    // 4. 4D→3D：方块 → 三角形
    {
        std::vector<Tri3D> allTris;
        allTris.reserve(visibleBlocks.size() * 12);

        for (const auto &blk : visibleBlocks)
        {
            COLORREF col = getBlockColor(blk.x, blk.y, blk.z, blk.w);
            blockToTriangles(blk.x, blk.y, blk.z, blk.w, cam, plane, col, allTris);
        }

        m_diagTriangles = static_cast<int>(allTris.size());

        if (allTris.empty()) goto blit;

        // 5. 设置 3D 相机（自适应场景）
        Camera3D cam3d;
        {
            double uMin, uMax, vMin, vMax, yMin, yMax;
            computeBounds(allTris, uMin, uMax, vMin, vMax, yMin, yMax);

            double cU = (uMin + uMax) * 0.5;
            double cV = (vMin + vMax) * 0.5;
            double cY = (yMin + yMax) * 0.5;

            double spanUV = std::max(uMax - uMin, vMax - vMin);
            double spanY = yMax - yMin;
            double dist = std::max(spanUV, spanY) * 2.5;
            if (dist < 1.0) dist = 10.0;

            cam3d.lookU = cU;
            cam3d.lookV = cV;
            cam3d.lookY = cY;
            cam3d.posU = cU + dist * 0.3;
            cam3d.posV = cV - dist * 0.6;
            cam3d.posY = cY + dist * 0.8;
        }

        // 6. 深度预排序
        std::sort(allTris.begin(), allTris.end(),
            [](const Tri3D &a, const Tri3D &b)
        {
            double da = (a.y[0] + a.y[1] + a.y[2]) / 3.0;
            double db = (b.y[0] + b.y[1] + b.y[2]) / 3.0;
            return da < db;
        });

        // 7. 光栅化
        rasterizeTriangles(allTris, cam3d);
    }

blit:
    // 8. 输出到屏幕
    HDC hdc = GetImageHDC();
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, m_memDC, 0, 0, SRCCOPY);

    // 9. FPS 统计
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

void Renderer::drawCrosshair() const
{
    int cx = m_screenWidth / 2;
    int cy = m_screenHeight / 2;
    setlinecolor(RGB(255, 255, 255));
    line(cx - 10, cy, cx + 10, cy);
    line(cx, cy - 10, cx, cy + 10);
}

// ============================================================================
// HUD
// ============================================================================

void Renderer::drawHUD(const Camera4D &cam) const
{
    wchar_t buf[256];
    settextcolor(RGB(200, 200, 200));
    setbkmode(TRANSPARENT);

    const Vec4 &p = cam.getPos();
    swprintf(buf, 256, L"Pos: (%.2f, %.2f, %.2f, %.2f)", p.x, p.y, p.z, p.w);
    outtextxy(10, 10, buf);

    swprintf(buf, 256, L"Over: (%.2f, %.2f, %.2f, %.2f)",
        cam.getOver().x, cam.getOver().y, cam.getOver().z, cam.getOver().w);
    outtextxy(10, 30, buf);

    swprintf(buf, 256, L"Pitch: %.2f  FPS: %d", cam.getPitch(), m_fps);
    outtextxy(10, 50, buf);

    swprintf(buf, 256, L"Blocks: %d vis / %d tri", m_diagVisible, m_diagTriangles);
    outtextxy(10, 70, buf);
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

        // 遮挡剔除
        bool occluded = true;
        if (!world.get(IVec4(bx + 1, by, bz, bw))) occluded = false;
        else if (!world.get(IVec4(bx - 1, by, bz, bw))) occluded = false;
        else if (!world.get(IVec4(bx, by + 1, bz, bw))) occluded = false;
        else if (!world.get(IVec4(bx, by - 1, bz, bw))) occluded = false;
        else if (!world.get(IVec4(bx, by, bz + 1, bw))) occluded = false;
        else if (!world.get(IVec4(bx, by, bz - 1, bw))) occluded = false;
        else if (!world.get(IVec4(bx, by, bz, bw + 1))) occluded = false;
        else if (!world.get(IVec4(bx, by, bz, bw - 1))) occluded = false;
        if (occluded) continue;

        if (blockIntersectsPlane(bx, by, bz, bw, camPos, plane, m_blockHalf, sp))
            result.push_back(IVec4(bx, by, bz, bw));
    }

    m_diagBlocks = static_cast<int>(result.size());
    return result;
}

void Renderer::traverseSuperBlock(const SuperBlock &sb, const Camera4D &cam,
    const Plane2D &plane, const World &world,
    std::vector<IVec4> &outBlocks)
{
    int baseX = sb.pos().x * SuperBlock::SIZE;
    int baseY = sb.pos().y * SuperBlock::SIZE;
    int baseZ = sb.pos().z * SuperBlock::SIZE;
    int baseW = sb.pos().w * SuperBlock::SIZE;

    const Vec4 &camPos = cam.getPos();
    double sp = m_blockHalf * 2.0;
    double overAbsSum = std::abs(plane.n.x) + std::abs(plane.n.z) + std::abs(plane.n.w);

    std::function<void(int, int, int, int, int)> traverse =
        [&](int bx, int by, int bz, int bw, int size)
    {
        double cs = size * sp;
        double cx = (bx + size * 0.5) * sp - camPos.x;
        double cz = (bz + size * 0.5) * sp - camPos.z;
        double cw = (bw + size * 0.5) * sp - camPos.w;
        double cod = plane.n.x * cx + plane.n.z * cz + plane.n.w * cw;
        if (std::abs(cod) > cs * overAbsSum + 1e-9) return;

        if (size == 1)
        {
            int lx = bx - baseX, ly = by - baseY;
            int lz = bz - baseZ, lw = bw - baseW;

            // 内部方块剔除
            if (lx > 0 && lx < SuperBlock::SIZE - 1 &&
                ly > 0 && ly < SuperBlock::SIZE - 1 &&
                lz > 0 && lz < SuperBlock::SIZE - 1 &&
                lw > 0 && lw < SuperBlock::SIZE - 1) return;

            // 边界方块遮挡检查
            auto exists = [&](int nx, int ny, int nz, int nw) -> bool
            {
                int lx2 = nx - baseX, ly2 = ny - baseY;
                int lz2 = nz - baseZ, lw2 = nw - baseW;
                if (lx2 >= 0 && lx2 < SuperBlock::SIZE &&
                    ly2 >= 0 && ly2 < SuperBlock::SIZE &&
                    lz2 >= 0 && lz2 < SuperBlock::SIZE &&
                    lw2 >= 0 && lw2 < SuperBlock::SIZE)
                    return true;
                if (m_sbGrid.count(IVec4(nx / SuperBlock::SIZE, ny / SuperBlock::SIZE,
                    nz / SuperBlock::SIZE, nw / SuperBlock::SIZE)))
                    return true;
                return world.get(IVec4(nx, ny, nz, nw)) != 0;
            };

            bool occluded = true;
            if (lx == 0) { if (!exists(bx - 1, by, bz, bw)) occluded = false; }
            if (lx == SuperBlock::SIZE - 1) { if (!exists(bx + 1, by, bz, bw)) occluded = false; }
            if (ly == 0) { if (!exists(bx, by - 1, bz, bw)) occluded = false; }
            if (ly == SuperBlock::SIZE - 1) { if (!exists(bx, by + 1, bz, bw)) occluded = false; }
            if (lz == 0) { if (!exists(bx, by, bz - 1, bw)) occluded = false; }
            if (lz == SuperBlock::SIZE - 1) { if (!exists(bx, by, bz + 1, bw)) occluded = false; }
            if (lw == 0) { if (!exists(bx, by, bz, bw - 1)) occluded = false; }
            if (lw == SuperBlock::SIZE - 1) { if (!exists(bx, by, bz, bw + 1)) occluded = false; }
            if (occluded) return;

            if (blockIntersectsPlane(bx, by, bz, bw, camPos, plane, m_blockHalf, sp))
                outBlocks.push_back(IVec4(bx, by, bz, bw));
            return;
        }

        int half = size / 2;
        for (int dx = 0; dx < 2; ++dx)
            for (int dy = 0; dy < 2; ++dy)
                for (int dz = 0; dz < 2; ++dz)
                    for (int dw = 0; dw < 2; ++dw)
                        traverse(bx + dx * half, by + dy * half,
                            bz + dz * half, bw + dw * half, half);
    };

    traverse(baseX, baseY, baseZ, baseW, SuperBlock::SIZE);
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

    double yLow = by * sp - half;
    double yHigh = by * sp + half;

    int n = poly.n;
    const auto &pu = poly.u;
    const auto &pv = poly.v;

    // 顶面和底面：扇形三角剖分（各 n-2 个三角形）
    for (int i = 1; i < n - 1; ++i)
    {
        // 顶面
        Tri3D t;
        t.u[0] = pu[0];     t.v[0] = pv[0];     t.y[0] = yHigh;
        t.u[1] = pu[i];     t.v[1] = pv[i];     t.y[1] = yHigh;
        t.u[2] = pu[i + 1]; t.v[2] = pv[i + 1]; t.y[2] = yHigh;
        t.color = color;
        t.depth = yHigh;
        outTris.push_back(t);

        // 底面（反转绕序使法线朝下）
        Tri3D b;
        b.u[0] = pu[0];     b.v[0] = pv[0];     b.y[0] = yLow;
        b.u[2] = pu[i];     b.v[2] = pv[i];     b.y[2] = yLow;
        b.u[1] = pu[i + 1]; b.v[1] = pv[i + 1]; b.y[1] = yLow;
        b.color = RGB(
            (GetRValue(color) * 2) / 3,
            (GetGValue(color) * 2) / 3,
            (GetBValue(color) * 2) / 3);
        b.depth = yLow;
        outTris.push_back(b);
    }

    // 侧面：n 条边，每条边 2 个三角形
    for (int i = 0; i < n; ++i)
    {
        int j = (i + 1) % n;
        double shade = 0.75;
        COLORREF sideCol = RGB(
            static_cast<int>(GetRValue(color) * shade),
            static_cast<int>(GetGValue(color) * shade),
            static_cast<int>(GetBValue(color) * shade));

        Tri3D t1;
        t1.u[0] = pu[i]; t1.v[0] = pv[i]; t1.y[0] = yLow;
        t1.u[1] = pu[j]; t1.v[1] = pv[j]; t1.y[1] = yLow;
        t1.u[2] = pu[i]; t1.v[2] = pv[i]; t1.y[2] = yHigh;
        t1.color = sideCol;
        t1.depth = (yLow + yHigh) * 0.5;
        outTris.push_back(t1);

        Tri3D t2;
        t2.u[0] = pu[j]; t2.v[0] = pv[j]; t2.y[0] = yLow;
        t2.u[1] = pu[j]; t2.v[1] = pv[j]; t2.y[1] = yHigh;
        t2.u[2] = pu[i]; t2.v[2] = pv[i]; t2.y[2] = yHigh;
        t2.color = sideCol;
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
