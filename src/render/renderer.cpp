#include "renderer.h"
#include "../core/constant.h"
#include "thread_pool.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <windows.h>

// NOTE EasyX PNG 像素格式为 0xAARRGGBB，但 DIB 是 0x00RRGGBB
static inline DWORD alphaBlend(DWORD dst, DWORD src)
{
    // 古战场打卡，这个 alphaBlend 写了我一整天，EasyX 的文档太少了
    unsigned int a = (src >> 24) & 0xFF;
    if (a == 0) return dst;
    if (a == 255) return src & 0x00FFFFFF;

    unsigned int sr = (src >> 16) & 0xFF;
    unsigned int sg = (src >> 8) & 0xFF;
    unsigned int sb = src & 0xFF;
    unsigned int dr = (dst >> 16) & 0xFF;
    unsigned int dg = (dst >> 8) & 0xFF;
    unsigned int db = dst & 0xFF;
    unsigned int invA = 255 - a;
    dr = (sr * a + dr * invA) / 255;
    dg = (sg * a + dg * invA) / 255;
    db = (sb * a + db * invA) / 255;
    return RGB(dr, dg, db);
}

Renderer::Renderer(int screenWidth, int screenHeight)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_blockHalf(0.5)
    , m_frameCount(0)
    // FIXME hardware_concurrency 在虚拟机可能返回 1
    , m_pool(new ThreadPool(std::max(1u, std::thread::hardware_concurrency())))
    , m_hBmp(nullptr)
    , m_memDC(nullptr)
    , m_oldBmp(nullptr)
    , m_dibReady(false)
    , m_hFont(nullptr)
    , m_hFontLarge(nullptr)
    , m_hOldFont(nullptr)
    , m_fpsFrames(0)
    , m_fpsTime(0)
    , m_fps(0)
    , m_diagTotal(0)
    , m_diagSlice(0)
    , m_diagOccl(0)
    , m_diagGeom(0)
    , m_diagFaces(0)
    , m_diagFaceCull(0)
    , m_diagChunkTotal(0)
    , m_diagChunkPass(0)
    , m_diagThreads(0)
    , m_diagTiles(0)
    , m_msCollect(0.0)
    , m_msFrustum(0.0)
    , m_msBlock2Tri(0.0)
    , m_msRaster(0.0)
    , m_blockTexLoaded(false)
{
    m_zbuf.resize(m_screenWidth * m_screenHeight);
    // 创建 DIB 离屏缓冲（不用 EasyX 默认缓冲因为需要直接操控像素）
    memset(m_texPixels, 0, sizeof(m_texPixels));
    memset(m_texW, 0, sizeof(m_texW));
    memset(m_texH, 0, sizeof(m_texH));

    HDC hdc = GetImageHDC();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_screenWidth;
    bmi.bmiHeader.biHeight = -m_screenHeight;  // 负数 = top-down DIB
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

        m_hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Minecraft AE");
        m_hFontLarge = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Minecraft AE");
        if (m_hFont)
            m_hOldFont = (HFONT) SelectObject(m_memDC, m_hFont);
    }
}

Renderer::~Renderer()
{
    delete m_pool;
    if (m_dibReady)
    {

        if (m_hOldFont)
            SelectObject(m_memDC, m_hOldFont);
        if (m_hFont)
            DeleteObject(m_hFont);
        if (m_hFontLarge)
            DeleteObject(m_hFontLarge);

        SelectObject(m_memDC, m_oldBmp);
        DeleteDC(m_memDC);
        DeleteObject(m_hBmp);
    }
}

void Renderer::resetBuffers()
{
    // TODO: memset 比 std::fill 快，但 1e30 转 float 可能有精度问题
    std::fill(m_zbuf.begin(), m_zbuf.end(), 1e30);
}

void Renderer::renderWorld(const World &world, const Camera4D &cam)
{
    if (!m_dibReady) return;

    ++m_frameCount;

    resetBuffers();

    // 天空/虚空渐变
    // HACK: 天空色写死在代码里，以后抽成 JSON
    {
        double pitch = cam.getPitch();
        constexpr double fov = 1.0472;
        constexpr double horizonOffset = 0.785;
        DWORD *bits = m_pBits;
        for (int y = 0; y < m_screenHeight; ++y)
        {
            double t = 0.5 - (double) y / (m_screenHeight - 1);
            double angle = pitch + t * fov + horizonOffset;

            int r, g, b;

            double blend = angle / 0.14;
            if (blend > 1.0)  blend = 1.0;
            if (blend < 0.0)  blend = 0.0;

            double w = blend * blend * (3.0 - 2.0 * blend);

            double s = (angle + 0.5) / 1.8;  if (s > 1.0) s = 1.0; if (s < 0.0) s = 0.0;
            int sr = (int) (180 * (1.0 - s) + 60 * s);
            int sg = (int) (220 * (1.0 - s) + 140 * s);
            int sb = (int) (255 * (1.0 - s) + 230 * s);

            double v = -(angle - 0.5) / 1.8; if (v > 1.0) v = 1.0; if (v < 0.0) v = 0.0;
            int vr = (int) (30 * (1.0 - v) + 2 * v);
            int vg = (int) (30 * (1.0 - v) + 2 * v);
            int vb = (int) (50 * (1.0 - v) + 8 * v);

            r = (int) (sr * w + vr * (1.0 - w));
            g = (int) (sg * w + vg * (1.0 - w));
            b = (int) (sb * w + vb * (1.0 - w));

            DWORD color = (r << 16) | (g << 8) | b;
            DWORD *row = bits + y * m_screenWidth;
            for (int x = 0; x < m_screenWidth; ++x)
                row[x] = color;
        }
    }

    Plane2D plane = cam.getViewPlane();

    m_diagTotal = static_cast<int>(world.totalBlocks());
    m_diagSlice = 0;
    m_diagOccl = 0;
    m_diagGeom = 0;
    m_diagFaces = 0;
    m_diagFaceCull = 0;

    clock_t t0 = clock();
    int preOccl = 0;
    std::vector<IVec4> visibleBlocks = collectVisibleBlocks(world, cam, plane, preOccl);
    m_msCollect = static_cast<double>(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
    m_diagSlice = preOccl;
    m_diagOccl = preOccl - static_cast<int>(visibleBlocks.size());

    if (!visibleBlocks.empty())
    {

        Camera3D cam3d;
        {
            double pitch = cam.getPitch();
            double sP = std::sin(pitch), cP = std::cos(pitch);

            cam3d.posU = 0.0;
            cam3d.posV = 0.0;
            cam3d.posY = 0.0;
            cam3d.dirU = 0.0;
            cam3d.dirV = cP;
            cam3d.dirY = sP;
        }

        {
            std::vector<Tri3D> allTris;
            allTris.reserve(visibleBlocks.size() * 12);

            double half = m_blockHalf, sp = half * 2.0;
            const Vec4 &camPos = cam.getPos();

            double rU = cam3d.dirV, rV = -cam3d.dirU;
            double rLen = std::sqrt(rU * rU + rV * rV);
            rU /= rLen; rV /= rLen;
            double upU = rV * cam3d.dirY, upV = -rU * cam3d.dirY, upY = rU * cam3d.dirV - rV * cam3d.dirU;

            clock_t tLoop = clock();

            size_t totalBlk = visibleBlocks.size();
            int numThreads = m_pool->workerCount();
            if (numThreads > 16) numThreads = 16;
            if (totalBlk < 16) numThreads = 1;

            std::vector<std::vector<Tri3D>> threadTris(numThreads);
            std::vector<int> threadGeom(numThreads, 0);

            m_pool->parallelRanges((int) totalBlk, numThreads,
                [&](int t, int start, int end)
            {
                auto &localTris = threadTris[t];
                localTris.reserve((end - start) * 12);
                int localGeom = 0;
                for (int i = start; i < end; ++i)
                {
                    const auto &blk = visibleBlocks[i];
                    double bu = vec3Dot(Vec3(blk.x * sp - camPos.x, blk.z * sp - camPos.z, blk.w * sp - camPos.w), plane.p);
                    double bv = vec3Dot(Vec3(blk.x * sp - camPos.x, blk.z * sp - camPos.z, blk.w * sp - camPos.w), plane.q);
                    double by = blk.y * sp - camPos.y;
                    double dU = bu - cam3d.posU;
                    double dV = bv - cam3d.posV;
                    double dY = by - cam3d.posY;
                    double camZ = cam3d.dirU * dU + cam3d.dirV * dV + cam3d.dirY * dY;
                    if (camZ < cam3d.nearPlane || camZ > cam3d.farPlane) continue;
                    double margin = half * 3.0;
                    double camX = rU * dU + rV * dV;
                    double camY = upU * dU + upV * dV + upY * dY;
                    double halfH = std::tan(cam3d.fov * 0.5) * camZ;
                    double halfW = halfH * m_screenWidth / m_screenHeight;
                    if (camX < -halfW - margin || camX > halfW + margin) continue;
                    if (camY < -halfH - margin || camY > halfH + margin) continue;
                    int bt = world.get(blk);
                    int topId = blockTexId(bt, 0);
                    int sideId = blockTexId(bt, 1);
                    int bottomId = blockTexId(bt, 2);
                    int ds = (m_miningStage >= 0 && blk == m_miningTarget) ? m_miningStage : -1;
                    size_t before = localTris.size();
                    blockToTriangles(blk.x, blk.y, blk.z, blk.w, cam, plane,
                        topId, sideId, bottomId, localTris, world, ds);
                    if (localTris.size() > before) ++localGeom;
                }
                threadGeom[t] = localGeom;
            });

            m_diagThreads = numThreads;

            m_diagGeom = 0;
            for (int t = 0; t < numThreads; ++t)
            {
                m_diagGeom += threadGeom[t];
                auto &src = threadTris[t];
                if (!src.empty())
                    allTris.insert(allTris.end(), src.begin(), src.end());
            }

            m_msBlock2Tri = static_cast<double>(clock() - tLoop) * 1000.0 / CLOCKS_PER_SEC;
            m_msFrustum = 0.0;

            m_diagFaces = static_cast<int>(allTris.size());

            if (!allTris.empty())
            {

                clock_t t2 = clock();
                int numTiles = m_pool->workerCount();
                if (numTiles > 8) numTiles = 8;
                if (allTris.size() < 16) numTiles = 1;

                m_pool->parallelRanges(m_screenHeight, numTiles,
                    [&](int, int ty0, int ty1)
                {
                    rasterizeTriangles(allTris, cam3d, ty0, ty1);
                });

                m_diagTiles = numTiles;
                m_msRaster = static_cast<double>(clock() - t2) * 1000.0 / CLOCKS_PER_SEC;
            }
        }
    }

    if (m_hasTarget)
        drawBlockOutline(m_targetBlock, cam);

    HDC hdc = GetImageHDC();
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, m_memDC, 0, 0, SRCCOPY);

    drawHUD(cam);
    drawCrosshair();

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

static bool blockIntersectsPlane(int bx, int, int bz, int bw,
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
    const Camera4D &cam, const Plane2D &plane,
    int &outPreOccl)
{
    std::vector<IVec4> result;
    outPreOccl = 0;

    const Vec4 &camPos = cam.getPos();
    double sp = m_blockHalf * 2.0;

    int chunksTotal = 0, chunksPass = 0;
    for (const auto &[chunkPos, chunk] : world.getChunks())
    {
        if (chunk.empty()) continue;
        ++chunksTotal;

        {
            double half = m_blockHalf;
            int sz = World::CHUNK_SIZE;
            double cx0 = (chunk.cx() * sz) * sp - camPos.x - half;
            double cx1 = (chunk.cx() * sz + sz - 1) * sp - camPos.x + half;
            double cz0 = (chunk.cz() * sz) * sp - camPos.z - half;
            double cz1 = (chunk.cz() * sz + sz - 1) * sp - camPos.z + half;
            double cw0 = (chunk.cw() * sz) * sp - camPos.w - half;
            double cw1 = (chunk.cw() * sz + sz - 1) * sp - camPos.w + half;

            double cmin = 0, cmax = 0;
            double nx = plane.n.x, nz = plane.n.z, nw = plane.n.w;
            if (nx > 0) { cmin += nx * cx0; cmax += nx * cx1; }
            else { cmin += nx * cx1; cmax += nx * cx0; }
            if (nz > 0) { cmin += nz * cz0; cmax += nz * cz1; }
            else { cmin += nz * cz1; cmax += nz * cz0; }
            if (nw > 0) { cmin += nw * cw0; cmax += nw * cw1; }
            else { cmin += nw * cw1; cmax += nw * cw0; }

            if (cmin > 0.0 || cmax < 0.0) continue;
        }
        ++chunksPass;

        for (const auto &[localPos, type] : chunk.blocks())
        {
            IVec4 blk = chunk.localToWorld(localPos.x, localPos.y, localPos.z, localPos.w);
            int bx = blk.x, by = blk.y, bz = blk.z, bw = blk.w;

            if (!blockIntersectsPlane(bx, by, bz, bw, camPos, plane, m_blockHalf, sp))
                continue;
            ++outPreOccl;

            if (world.get(IVec4(bx + 1, by, bz, bw)) &&
                world.get(IVec4(bx - 1, by, bz, bw)) &&
                world.get(IVec4(bx, by + 1, bz, bw)) &&
                world.get(IVec4(bx, by - 1, bz, bw)) &&
                world.get(IVec4(bx, by, bz + 1, bw)) &&
                world.get(IVec4(bx, by, bz - 1, bw)) &&
                world.get(IVec4(bx, by, bz, bw + 1)) &&
                world.get(IVec4(bx, by, bz, bw - 1)))
                continue;

            result.push_back(IVec4(bx, by, bz, bw));
        }
    }

    m_diagChunkTotal = chunksTotal;
    m_diagChunkPass = chunksPass;

    if (result.size() > 1)
    {
        Vec4 over = cam.getOver();
        std::sort(result.begin(), result.end(),
            [&over](const IVec4 &a, const IVec4 &b)
        {
            double da = a.x * over.x + a.y * over.y + a.z * over.z + a.w * over.w;
            double db = b.x * over.x + b.y * over.y + b.z * over.z + b.w * over.w;
            return da < db;
        });
    }

    return result;
}

void Renderer::drawBlockOutline(const IVec4 &blockPos, const Camera4D &cam)
{
    const Vec4 &camPos = cam.getPos();
    double half = m_blockHalf;
    double sp = half * 2.0;

    int bx = blockPos.x, by = blockPos.y, bz = blockPos.z, bw = blockPos.w;

    double x0 = bx * sp - camPos.x - half;
    double x1 = bx * sp - camPos.x + half;
    double z0 = bz * sp - camPos.z - half;
    double z1 = bz * sp - camPos.z + half;
    double w0 = bw * sp - camPos.w - half;
    double w1 = bw * sp - camPos.w + half;

    Plane2D camPlane = cam.getViewPlane();
    camPlane.offset = 0.0;

    PolyOnPlane poly = intersectCubePlane(x0, x1, z0, z1, w0, w1, camPlane);
    if (!poly.valid()) return;

    double yLow = by * sp - camPos.y - half;
    double yHigh = by * sp - camPos.y + half;
    int n = poly.n;

    Camera3D cam3d;
    {
        double pitch = cam.getPitch();
        double sP = std::sin(pitch), cP = std::cos(pitch);
        cam3d.posU = 0.0; cam3d.posV = 0.0; cam3d.posY = 0.0;
        cam3d.dirU = 0.0;
        cam3d.dirV = cP;
        cam3d.dirY = sP;
    }

    for (int i = 0; i < n; ++i)
    {
        int j = (i + 1) % n;

        drawOutlineEdge3D(poly.u[i], poly.v[i], yHigh,
            poly.u[j], poly.v[j], yHigh, cam3d);

        drawOutlineEdge3D(poly.u[i], poly.v[i], yLow,
            poly.u[j], poly.v[j], yLow, cam3d);

        drawOutlineEdge3D(poly.u[i], poly.v[i], yLow,
            poly.u[i], poly.v[i], yHigh, cam3d);
    }
}

void Renderer::drawOutlineEdge3D(double u0, double v0, double y0,
    double u1, double v1, double y1,
    const Camera3D &cam3d)
{
    int sx0, sy0, sx1, sy1;
    double z0, z1;

    if (!project3D(u0, v0, y0, cam3d, m_screenWidth, m_screenHeight, sx0, sy0, z0)) return;
    if (!project3D(u1, v1, y1, cam3d, m_screenWidth, m_screenHeight, sx1, sy1, z1)) return;

    int dx = std::abs(sx1 - sx0);
    int dy = std::abs(sy1 - sy0);
    int steps = std::max(dx, dy);
    if (steps <= 0) return;

    double invSteps = 1.0 / steps;
    for (int i = 0; i <= steps; ++i)
    {
        double t = i * invSteps;
        int sx = static_cast<int>(sx0 + (sx1 - sx0) * t + 0.5);
        int sy = static_cast<int>(sy0 + (sy1 - sy0) * t + 0.5);
        double z = z0 + (z1 - z0) * t;

        if (sx < 0 || sx >= m_screenWidth || sy < 0 || sy >= m_screenHeight)
            continue;

        int idx = sy * m_screenWidth + sx;
        if (z <= m_zbuf[idx])
        {
            m_pBits[idx] = RGB(0, 0, 0);
            m_zbuf[idx] = z;
        }
    }
}

void Renderer::blockToTriangles(int bx, int by, int bz, int bw,
    const Camera4D &cam, const Plane2D &plane,
    int topTexId, int sideTexId, int bottomTexId,
    std::vector<Tri3D> &outTris,
    const World &world, int destroyStage)
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

    Plane2D camPlane = plane;
    camPlane.offset = 0.0;

    PolyOnPlane poly = intersectCubePlane(x0, x1, z0, z1, w0, w1, camPlane);
    if (!poly.valid()) return;

    double yLow = by * sp - camPos.y - half;
    double yHigh = by * sp - camPos.y + half;

    bool cullTop = world.get(IVec4(bx, by + 1, bz, bw)) != 0;
    bool cullBottom = world.get(IVec4(bx, by - 1, bz, bw)) != 0;
    if (cullTop) ++m_diagFaceCull;
    if (cullBottom) ++m_diagFaceCull;

    int n = poly.n;
    const auto &pu = poly.u;
    const auto &pv = poly.v;
    const auto &ox = poly.ox;
    const auto &oz = poly.oz;
    double invSp = 1.0 / sp;

    if (!cullTop)
    {
        for (int i = 1; i < n - 1; ++i)
        {
            Tri3D t;
            t.u[0] = pu[0];     t.v[0] = pv[0];     t.y[0] = yHigh;
            t.u[1] = pu[i];     t.v[1] = pv[i];     t.y[1] = yHigh;
            t.u[2] = pu[i + 1]; t.v[2] = pv[i + 1]; t.y[2] = yHigh;
            t.tu[0] = (ox[0] - x0) * invSp;     t.tv[0] = (oz[0] - z0) * invSp;
            t.tu[1] = (ox[i] - x0) * invSp;     t.tv[1] = (oz[i] - z0) * invSp;
            t.tu[2] = (ox[i + 1] - x0) * invSp; t.tv[2] = (oz[i + 1] - z0) * invSp;
            t.color = RGB(128, 128, 128);
            t.depth = yHigh;
            t.texId = topTexId;
            t.destroyStage = destroyStage;
            outTris.push_back(t);
        }
    }

    if (!cullBottom)
    {
        for (int i = 1; i < n - 1; ++i)
        {
            Tri3D b;
            b.u[0] = pu[0];     b.v[0] = pv[0];     b.y[0] = yLow;
            b.u[2] = pu[i];     b.v[2] = pv[i];     b.y[2] = yLow;
            b.u[1] = pu[i + 1]; b.v[1] = pv[i + 1]; b.y[1] = yLow;
            b.tu[0] = (ox[0] - x0) * invSp;     b.tv[0] = (oz[0] - z0) * invSp;
            b.tu[2] = (ox[i] - x0) * invSp;     b.tv[2] = (oz[i] - z0) * invSp;
            b.tu[1] = (ox[i + 1] - x0) * invSp; b.tv[1] = (oz[i + 1] - z0) * invSp;
            b.color = RGB(128, 128, 128);
            b.depth = yLow;
            b.texId = bottomTexId;
            b.destroyStage = destroyStage;
            outTris.push_back(b);
        }
    }

    for (int i = 0; i < n; ++i)
    {
        int j = (i + 1) % n;

        Tri3D t1;
        t1.u[0] = pu[i]; t1.v[0] = pv[i]; t1.y[0] = yLow;
        t1.u[1] = pu[j]; t1.v[1] = pv[j]; t1.y[1] = yLow;
        t1.u[2] = pu[i]; t1.v[2] = pv[i]; t1.y[2] = yHigh;
        t1.tu[0] = 0.0; t1.tv[0] = 1.0;
        t1.tu[1] = 1.0; t1.tv[1] = 1.0;
        t1.tu[2] = 0.0; t1.tv[2] = 0.0;
        t1.color = RGB(128, 128, 128);
        t1.depth = (yLow + yHigh) * 0.5;
        t1.texId = sideTexId;
        t1.destroyStage = destroyStage;
        outTris.push_back(t1);

        Tri3D t2;
        t2.u[0] = pu[j]; t2.v[0] = pv[j]; t2.y[0] = yLow;
        t2.u[1] = pu[j]; t2.v[1] = pv[j]; t2.y[1] = yHigh;
        t2.u[2] = pu[i]; t2.v[2] = pv[i]; t2.y[2] = yHigh;
        t2.tu[0] = 1.0; t2.tv[0] = 1.0;
        t2.tu[1] = 1.0; t2.tv[1] = 0.0;
        t2.tu[2] = 0.0; t2.tv[2] = 0.0;
        t2.color = RGB(128, 128, 128);
        t2.depth = (yLow + yHigh) * 0.5;
        t2.texId = sideTexId;
        t2.destroyStage = destroyStage;
        outTris.push_back(t2);
    }
}

void Renderer::rasterizeTriangles(const std::vector<Tri3D> &tris,
    const Camera3D &cam3d, int tileYMin, int tileYMax)
{
    for (const auto &tri : tris)
        rasterizeTriangle(tri, cam3d, tileYMin, tileYMax);
}

void Renderer::rasterizeTriangle(const Tri3D &tri, const Camera3D &cam3d,
    int tileYMin, int tileYMax)
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

    int idx[3] = { 0, 1, 2 };
    if (sy[idx[0]] > sy[idx[1]]) std::swap(idx[0], idx[1]);
    if (sy[idx[1]] > sy[idx[2]]) std::swap(idx[1], idx[2]);
    if (sy[idx[0]] > sy[idx[1]]) std::swap(idx[0], idx[1]);

    int y0 = sy[idx[0]], y1 = sy[idx[1]], y2 = sy[idx[2]];
    int x0 = sx[idx[0]], x1 = sx[idx[1]], x2 = sx[idx[2]];
    double z0 = sz[idx[0]], z1 = sz[idx[1]], z2 = sz[idx[2]];

    double tu0 = tri.tu[idx[0]], tu1 = tri.tu[idx[1]], tu2 = tri.tu[idx[2]];
    double tv0 = tri.tv[idx[0]], tv1 = tri.tv[idx[1]], tv2 = tri.tv[idx[2]];
    double tuz0 = tu0 / z0, tuz1 = tu1 / z1, tuz2 = tu2 / z2;
    double tvz0 = tv0 / z0, tvz1 = tv1 / z1, tvz2 = tv2 / z2;
    double ooz0 = 1.0 / z0, ooz1 = 1.0 / z1, ooz2 = 1.0 / z2;

    if (y2 < tileYMin || y0 >= tileYMax) return;
    if (y0 == y2) return;

    if (y1 > y0)
    {
        double invDyTop = 1.0 / static_cast<double>(y1 - y0);
        double invDyFull = 1.0 / static_cast<double>(y2 - y0);
        double dxL = static_cast<double>(x1 - x0) * invDyTop;
        double dxR = static_cast<double>(x2 - x0) * invDyFull;
        double dzL = (z1 - z0) * invDyTop;
        double dzR = (z2 - z0) * invDyFull;
        double dtuL = (tuz1 - tuz0) * invDyTop, dtuR = (tuz2 - tuz0) * invDyFull;
        double dtvL = (tvz1 - tvz0) * invDyTop, dtvR = (tvz2 - tvz0) * invDyFull;
        double doL = (ooz1 - ooz0) * invDyTop, doR = (ooz2 - ooz0) * invDyFull;

        int yStart = std::max({ y0, 0, tileYMin });
        int yEnd = std::min({ y1, m_screenHeight, tileYMax });

        for (int y = yStart; y < yEnd; ++y)
        {
            double t = static_cast<double>(y - y0);
            int xL = static_cast<int>(x0 + dxL * t);
            int xR = static_cast<int>(x0 + dxR * t);
            double zL = z0 + dzL * t, zR = z0 + dzR * t;
            double tuL = tuz0 + dtuL * t, tuR = tuz0 + dtuR * t;
            double tvL = tvz0 + dtvL * t, tvR = tvz0 + dtvR * t;
            double ooL = ooz0 + doL * t, ooR = ooz0 + doR * t;
            if (xL > xR) { std::swap(xL, xR); std::swap(zL, zR); std::swap(tuL, tuR); std::swap(tvL, tvR); std::swap(ooL, ooR); }
            drawScanline(y, xL, xR, zL, zR, tuL, tvL, ooL, tuR, tvR, ooR, tri.texId, tri.color, tri.destroyStage, tileYMin, tileYMax);
        }
    }

    if (y2 > y1)
    {
        double invDyBot = 1.0 / static_cast<double>(y2 - y1);
        double invDyFull = 1.0 / static_cast<double>(y2 - y0);
        double dxL = static_cast<double>(x2 - x0) * invDyFull;
        double dxR = static_cast<double>(x2 - x1) * invDyBot;
        double dzL = (z2 - z0) * invDyFull;
        double dzR = (z2 - z1) * invDyBot;
        double dtuL = (tuz2 - tuz0) * invDyFull, dtuR = (tuz2 - tuz1) * invDyBot;
        double dtvL = (tvz2 - tvz0) * invDyFull, dtvR = (tvz2 - tvz1) * invDyBot;
        double doL = (ooz2 - ooz0) * invDyFull, doR = (ooz2 - ooz1) * invDyBot;

        int yStart = std::max({ y1, 0, tileYMin });
        int yEnd = std::min({ y2, m_screenHeight, tileYMax });

        for (int y = yStart; y < yEnd; ++y)
        {
            double tFull = static_cast<double>(y - y0);
            double tBot = static_cast<double>(y - y1);
            int xL = static_cast<int>(x0 + dxL * tFull);
            int xR = static_cast<int>(x1 + dxR * tBot);
            double zL = z0 + dzL * tFull, zR = z1 + dzR * tBot;
            double tuL = tuz0 + dtuL * tFull, tuR = tuz1 + dtuR * tBot;
            double tvL = tvz0 + dtvL * tFull, tvR = tvz1 + dtvR * tBot;
            double ooL = ooz0 + doL * tFull, ooR = ooz1 + doR * tBot;
            if (xL > xR) { std::swap(xL, xR); std::swap(zL, zR); std::swap(tuL, tuR); std::swap(tvL, tvR); std::swap(ooL, ooR); }
            drawScanline(y, xL, xR, zL, zR, tuL, tvL, ooL, tuR, tvR, ooR, tri.texId, tri.color, tri.destroyStage, tileYMin, tileYMax);
        }
    }
}

void Renderer::drawScanline(int y, int x0, int x1, double z0, double z1,
    double tu0, double tv0, double ooz0, double tu1, double tv1, double ooz1,
    int texId, COLORREF color, int destroyStage, int tileYMin, int tileYMax)
{
    if (y < tileYMin || y >= tileYMax) return;
    if (y < 0 || y >= m_screenHeight) return;

    if (x0 < 0)
    {
        int origX1 = x1; if (origX1 <= x0) return;
        double t = (0.0 - x0) / (double) (origX1 - x0);
        z0 += (z1 - z0) * t;
        tu0 += (tu1 - tu0) * t;
        tv0 += (tv1 - tv0) * t;
        ooz0 += (ooz1 - ooz0) * t;
        x0 = 0;
    }
    if (x1 > m_screenWidth)
    {
        int origX0 = x0; if (x1 <= origX0) return;
        double t = (double) (m_screenWidth - origX0) / (double) (x1 - origX0);
        z1 = z0 + (z1 - z0) * t;
        tu1 = tu0 + (tu1 - tu0) * t;
        tv1 = tv0 + (tv1 - tv0) * t;
        ooz1 = ooz0 + (ooz1 - ooz0) * t;
        x1 = m_screenWidth;
    }
    if (x0 >= x1) return;

    int rowBase = y * m_screenWidth;
    double invDx = 1.0 / static_cast<double>(x1 - x0);
    double dz = (z1 - z0) * invDx;
    double dtu = (tu1 - tu0) * invDx;
    double dtv = (tv1 - tv0) * invDx;
    double doo = (ooz1 - ooz0) * invDx;

    for (int x = x0; x < x1; ++x)
    {
        double t = static_cast<double>(x - x0);
        double z = z0 + dz * t;
        int idx = rowBase + x;
        if (z < m_zbuf[idx])
        {
            m_zbuf[idx] = z;
            if (texId >= 0)
            {
                double oo = ooz0 + doo * t;
                double tu = (tu0 + dtu * t) / oo;
                double tv = (tv0 + dtv * t) / oo;
                m_pBits[idx] = sampleTexture(texId, tu, tv);

                if (destroyStage >= 0 && destroyStage < DESTROY_STAGES && m_destroyLoaded)
                {
                    int dtx = (int) (tu * 16.0) & 15;
                    int dty = (int) (tv * 16.0) & 15;
                    DWORD dc = m_destroyPixels[destroyStage][dty][dtx];

                    int alpha = (int) ((dc >> 24) & 0xFF);
                    if (alpha > 0)
                    {

                        int gray = ((int) (dc & 0xFF) + (int) ((dc >> 8) & 0xFF) + (int) ((dc >> 16) & 0xFF)) / 3;

                        double strength = (alpha / 255.0) * (1.0 - gray / 255.0);
                        double factor = 1.0 - strength;
                        DWORD base = m_pBits[idx];
                        int r = (int) ((base & 0xFF) * factor);
                        int g = (int) (((base >> 8) & 0xFF) * factor);
                        int b = (int) (((base >> 16) & 0xFF) * factor);
                        m_pBits[idx] = RGB(r, g, b);
                    }
                }
            }
            else
                m_pBits[idx] = color;
        }
    }
}