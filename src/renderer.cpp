#include "renderer.h"
#include <algorithm>
#include <vector>
#include <utility>
#include <cstdio>
#include <cwchar>
#include <iostream>
#include <windows.h>

// ============================================================================
// 超立方体 24 个二维面（每个面 4 个顶点，构成一个正方形�?
// ============================================================================
const int Renderer::FACES[24][4] = {
    // XY 面（z,w 固定，变�?bit0,bit1：顺�?0-1-3-2�?
    {0,1,3,2}, {4,5,7,6}, {8,9,11,10}, {12,13,15,14},
    // XZ 面（y,w 固定，变�?bit0,bit2：顺�?0-1-5-4�?
    {0,1,5,4}, {2,3,7,6}, {8,9,13,12}, {10,11,15,14},
    // XW 面（y,z 固定，变�?bit0,bit3：顺�?0-2-6-4�?
    {0,2,6,4}, {1,3,7,5}, {8,10,14,12}, {9,11,15,13},
    // YZ 面（x,w 固定，变�?bit1,bit2：顺�?0-2,3,1  �?0-2-10-8? 不对，变�?bit1,bit2�?
    // 顶点: 0(0000), 2(0100), 10(1010), 8(1000) �?顺序 0-2-10-8
    {0,2,10,8}, {1,3,11,9}, {4,6,14,12}, {5,7,15,13},
    // YW 面（x,z 固定，变�?bit1,bit3：顺�?0-1-9-8�?
    {0,1,9,8}, {2,3,11,10}, {4,5,13,12}, {6,7,15,14},
    // ZW 面（x,y 固定，变�?bit2,bit3：顺�?0-4-12-8�?
    {0,4,12,8}, {1,5,13,9}, {2,6,14,10}, {3,7,15,11}
};

// 生成 16 个顶�?
static void hypercubeVertices(int bx, int by, int bz, int bw, Vec4 v[16], double half)
{
    double sp = half * 2.0;  // 间距 = 2×半边长，保证相邻方块无间�?
    double cx = static_cast<double>(bx) * sp;
    double cy = static_cast<double>(by) * sp;
    double cz = static_cast<double>(bz) * sp;
    double cw = static_cast<double>(bw) * sp;
    for (int i = 0; i < 16; ++i)
    {
        double sx = (i & 1) ? half : -half;
        double sy = (i & 2) ? half : -half;
        double sz = (i & 4) ? half : -half;
        double sw = (i & 8) ? half : -half;
        v[i] = Vec4(cx + sx, cy + sy, cz + sz, cw + sw);
    }
}

// ============================================================================
// 构�?
// ============================================================================

Renderer::Renderer(int screenWidth, int screenHeight, double scale)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_scale(scale)
    , m_offsetX(screenWidth / 2.0)
    , m_offsetY(screenHeight / 2.0)
    , m_blockHalf(0.5 / 16.0)
    , m_frameCount(0)
    , m_fpsFrames(0)
    , m_fpsTime(0)
    , m_fps(0)
    , m_texLoaded(false)
{
    m_zbuf.resize(m_screenWidth * m_screenHeight);
    memset(m_tex, 0, sizeof(m_tex));
}

// 从方块坐标生成伪随机颜色（纹理未加载时使用）
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

Renderer::~Renderer()
{}

COLORREF Renderer::getBlockColor(int x, int y, int z, int w) const
{
    if (m_texLoaded)
    {
        // 纹理坐标取模映射�?[0,15]
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
            // std::wcout << path << std::endl;
            IMAGE img;
            loadimage(&img, path);
            DWORD *buf = GetImageBuffer(&img);
            if (buf && img.getwidth() > 0)
            {
                int w = img.getwidth(), h = img.getheight();
                for (int y = 0; y < 16 && y < h; ++y)
                    for (int ww = 0; ww < 16 && ww < w; ++ww)
                        m_tex[x][15 - y][z][ww] = buf[y * w + ww];
                if (x == 0 && z == 0)
                    std::wcout << L"  loaded color[0][0][0][0]=0x" << std::hex << m_tex[0][0][0][0] << std::dec << L" w=" << w << L" h=" << h << std::endl;
            }
            else
            {
                std::wcout << L"  FAILED: w=" << img.getwidth() << L" buf=" << (buf ? L"ok" : L"null") << std::endl;
            }
        }
    }
    m_texLoaded = true;
}

// ============================================================================
// 渲染世界：先全部框线，再逐帧填充胞腔�?
// ============================================================================

void Renderer::renderWorld(const World &world, const Camera4D &cam)
{
    ++m_frameCount;
    resetBuffers();

    // 创建 32-bit DIB 位图
    HDC hdc = GetImageHDC();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_screenWidth;
    bmi.bmiHeader.biHeight = -m_screenHeight;  // 负�?= 自上而下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    DWORD *bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
        reinterpret_cast<void **>(&bits), nullptr, 0);
    if (!hBmp || !bits) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP oldBmp = (HBITMAP) SelectObject(memDC, hBmp);

    // 填充背景
    DWORD bg = 0x001E0A0A;  // RGB(10,10,30)
    int total = m_screenWidth * m_screenHeight;
    for (int i = 0; i < total; ++i) bits[i] = bg;

    // 使用临时帧缓冲指针绘�?
    m_pBits = bits;
    drawFacesStep(world, cam);
    m_pBits = nullptr;

    // 刷到屏幕
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    DeleteObject(hBmp);

    // FPS 统计
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

// 快速判断方块是否可能与切片相交
static bool mayIntersectSlice(int bx, int by, int bz, int bw,
    const Vec4 &camPos, const Vec4 &over, double half, double sp)
{
    double cx = bx * sp, cy = by * sp, cz = bz * sp, cw = bw * sp;
    double centerOD = over.x * (cx - camPos.x) + over.y * (cy - camPos.y)
        + over.z * (cz - camPos.z) + over.w * (cw - camPos.w);
    double extent = half * (std::abs(over.x) + std::abs(over.y)
        + std::abs(over.z) + std::abs(over.w));
    return std::abs(centerOD) <= extent + 1e-9;
}

void Renderer::drawFacesStep(const World &world, const Camera4D &cam)
{
    const auto &blocks = world.getAllBlocks();
    if (blocks.empty()) return;

    Vec4 camPos = cam.getPos();
    const Vec4 &ov = cam.getOver();

    // 收集所有方块的交线段，构建 (block, 胞腔) �?交点列表
    struct FaceData { int bx, by, bz, bw; COLORREF col; POINT pts[12]; double depths[12]; int n; };
    std::vector<FaceData> allFaces;

    for (const auto &pair : blocks)
    {
        int bx = pair.first.x, by = pair.first.y, bz = pair.first.z, bw = pair.first.w;
        // 快速剔除：切片不可能穿过此方块
        if (!mayIntersectSlice(bx, by, bz, bw, camPos, ov, m_blockHalf, m_blockHalf * 2.0))
            continue;
        // 遮挡检�?
        if (world.get(IVec4(bx + 1, by, bz, bw)) && world.get(IVec4(bx - 1, by, bz, bw)) &&
            world.get(IVec4(bx, by + 1, bz, bw)) && world.get(IVec4(bx, by - 1, bz, bw)) &&
            world.get(IVec4(bx, by, bz + 1, bw)) && world.get(IVec4(bx, by, bz - 1, bw)) &&
            world.get(IVec4(bx, by, bz, bw + 1)) && world.get(IVec4(bx, by, bz, bw - 1)))
            continue;

        COLORREF col = getBlockColor(bx, by, bz, bw);

        Vec4 verts[16]; hypercubeVertices(bx, by, bz, bw, verts, m_blockHalf);
        double od[16];
        for (int i = 0; i < 16; ++i)
            od[i] = vec4Dot(vec4Sub(verts[i], camPos), ov);

        // 收集交线�?
        struct Seg2 { Vec4 a, b; int faceIdx; };
        Seg2 segs[24]; int sc = 0;
        for (int f = 0; f < 24; ++f)
        {
            const int *face = FACES[f];
            Vec4 hits[4]; int hc = 0;
            for (int e = 0; e < 4 && hc < 4; ++e)
            {
                int a = face[e], b = face[(e + 1) & 3];
                double da = od[a], db = od[b];
                if ((da > 0 && db > 0) || (da < 0 && db < 0)) continue;
                double t = (std::abs(da - db) > 1e-12) ? da / (da - db) : 0.5;
                hits[hc++] = vec4Add(verts[a], vec4Scale(vec4Sub(verts[b], verts[a]), t));
            }
            if (hc == 2) segs[sc++] = { hits[0], hits[1], f };
        }
        if (sc == 0) continue;

        // 8 个胞�?
        struct Cell { int bit, val; };
        const Cell cells[8] = { {0,0},{0,1},{1,0},{1,1},{2,0},{2,1},{3,0},{3,1} };
        auto faceBits = [](const int *f) -> std::pair<int, int>
        {
            int v = f[0], b0 = -1, b1 = -1;
            for (int bit = 0; bit < 4; ++bit)
            {
                int m = 1 << bit; bool same = true;
                for (int i = 1; i < 4; ++i) if ((f[i] & m) != (v & m)) { same = false; break; }
                if (same) { if (b0 < 0) b0 = bit; else b1 = bit; }
            }
            return { b0,b1 };
        };

        for (int ci = 0; ci < 8; ++ci)
        {
            int cb = cells[ci].bit, cv = cells[ci].val;
            int cSegs[6]; int csc = 0;
            for (int s = 0; s < sc; ++s)
            {
                const int *f = FACES[segs[s].faceIdx];
                auto [b0, b1] = faceBits(f);
                bool ok = false;
                if (b0 == cb && ((f[0] >> cb) & 1) == cv) ok = true;
                if (b1 == cb && ((f[0] >> cb) & 1) == cv) ok = true;
                if (ok && csc < 6) cSegs[csc++] = s;
            }
            if (csc < 3) continue;

            // ---- 通过线段端点匹配构建正确的多边形顶点顺序 ----
            // 收集此胞腔所有线段的端点（世界坐标）
            struct EP { Vec4 pos; int segIdx; };  // 端点：位�?+ 所属线�?
            EP eps[12]; int epCount = 0;
            for (int i = 0; i < csc; ++i)
            {
                int si = cSegs[i];
                eps[epCount++] = { segs[si].a, i };
                eps[epCount++] = { segs[si].b, i };
            }

            // 匹配共享端点（同位置、不同线�?�?相邻顶点�?
            int next[12];  // next[i] = �?eps[i] 共享位置的另一个端点的索引
            for (int i = 0; i < epCount; ++i)
            {
                next[i] = -1;
                for (int j = 0; j < epCount; ++j)
                {
                    if (i == j) continue;
                    if (eps[i].segIdx == eps[j].segIdx) continue;  // 同线段不是邻�?
                    if (vec4DistSq(eps[i].pos, eps[j].pos) < 1e-6)
                    {
                        // j �?i 同位置但不同线段 �?i 的搭档是 j 所在线段的另一端点
                        int other = -1;
                        for (int k = 0; k < epCount; ++k)
                            if (k != j && eps[k].segIdx == eps[j].segIdx) { other = k; break; }
                        next[i] = other;
                        break;
                    }
                }
            }

            // 按顺序追踪链（从第一个有效的端点开始）
            POINT orderedPts[12]; double orderedDepths[12]; int orderedN = 0;
            bool used[12] = {};
            int cur = 0;
            while (cur >= 0 && !used[cur] && orderedN < 12)
            {
                used[cur] = true;
                ProjResult pr = project(eps[cur].pos, cam, m_scale, m_offsetX, m_offsetY, cam.getPitch());
                if (!pr.valid) break;
                orderedPts[orderedN] = { static_cast<int>(pr.screenPos.x), static_cast<int>(pr.screenPos.y) };
                Vec4 dv = vec4Sub(eps[cur].pos, cam.getPos());
                orderedDepths[orderedN] = vec4Dot(dv, cam.getForward());
                ++orderedN;
                cur = next[cur];
            }

            if (orderedN < 3) continue;

            FaceData fd = { bx,by,bz,bw, col, {}, {}, 0 };
            for (int i = 0; i < orderedN && fd.n < 12; ++i)
            {
                fd.pts[fd.n] = orderedPts[i];
                fd.depths[fd.n] = orderedDepths[i];
                ++fd.n;
            }
            allFaces.push_back(fd);
        }
    }

    // 逐帧显示：收集所有面及其深度，按深度排序后用深度缓冲绘制
    struct FaceWithDepth { FaceData fd; double avgDepth; };
    std::vector<FaceWithDepth> fds;
    for (auto &fd : allFaces)
    {
        double sumZ = 0;
        for (int i = 0; i < fd.n; ++i) sumZ += fd.depths[i];
        fds.push_back({ fd, sumZ / fd.n });
    }
    // 从远到近排序（painter 预排�?+ z-buffer�?
    std::sort(fds.begin(), fds.end(), [](const FaceWithDepth &a, const FaceWithDepth &b)
    {
        return a.avgDepth > b.avgDepth;
    });

    int showCount = static_cast<int>(fds.size());  // 全部显示
    for (int i = 0; i < static_cast<int>(fds.size()) && i < showCount; ++i)
    {
        const FaceData &fd = fds[i].fd;
        int r = GetRValue(fd.col), g = GetGValue(fd.col), b = GetBValue(fd.col);
        fillPolygonZ(fd.pts, fd.n, fd.depths, RGB(r, g, b));
    }
}

// ============================================================================
// 深度缓冲
// ============================================================================

void Renderer::resetBuffers()
{
    std::fill(m_zbuf.begin(), m_zbuf.end(), 1e100);
}

void Renderer::fillPolygonZ(const POINT *pts, int n, const double *depths, COLORREF color)
{
    if (n < 3) return;

    // 找多边形在屏幕上的包围盒
    int minY = pts[0].y, maxY = pts[0].y;
    for (int i = 1; i < n; ++i)
    {
        if (pts[i].y < minY) minY = pts[i].y;
        if (pts[i].y > maxY) maxY = pts[i].y;
    }
    if (minY < 0) minY = 0;
    if (maxY >= m_screenHeight) maxY = m_screenHeight - 1;
    if (minY > maxY) return;

    // 对每条扫描线，计算多边形交点
    for (int y = minY; y <= maxY; ++y)
    {
        // 找到与扫描线相交的所有边
        struct Edge { double x; double z; };
        Edge edges[12]; int ec = 0;

        for (int i = 0; i < n; ++i)
        {
            int j = (i + 1) % n;
            int y0 = pts[i].y, y1 = pts[j].y;
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y))
            {
                double t = static_cast<double>(y - y0) / static_cast<double>(y1 - y0);
                double x = pts[i].x + t * (pts[j].x - pts[i].x);
                double z = depths[i] + t * (depths[j] - depths[i]);
                if (ec < 12) edges[ec++] = { x, z };
            }
        }

        // �?x 排序
        for (int i = 0; i < ec; ++i)
            for (int j = i + 1; j < ec; ++j)
                if (edges[i].x > edges[j].x) std::swap(edges[i], edges[j]);

        // 逐对填充水平线段
        for (int k = 0; k + 1 < ec; k += 2)
        {
            int x0 = static_cast<int>(edges[k].x);
            int x1 = static_cast<int>(edges[k + 1].x);
            double z0 = edges[k].z;
            double z1 = edges[k + 1].z;

            if (x0 < 0) x0 = 0;
            if (x1 >= m_screenWidth) x1 = m_screenWidth - 1;
            if (x0 > x1) continue;

            double dz = (x1 > x0) ? (z1 - z0) / (x1 - x0) : 0;
            double z = z0;
            for (int x = x0; x <= x1; ++x, z += dz)
            {
                int idx = y * m_screenWidth + x;
                if (idx >= 0 && idx < static_cast<int>(m_zbuf.size()) && z < m_zbuf[idx])
                {
                    m_zbuf[idx] = z;
                    m_pBits[idx] = color;
                }
            }
        }
    }
}

// ============================================================================
// 十字准星
// ============================================================================

void Renderer::drawCrosshair() const
{
    int cx = m_screenWidth / 2;
    int cy = m_screenHeight / 2;
    int gap = 6;
    int len = 10;

    setlinecolor(RGB(255, 255, 255));
    // 水平�?
    line(cx - len, cy, cx - gap, cy);
    line(cx + gap, cy, cx + len, cy);
    // 垂直�?
    line(cx, cy - len, cx, cy - gap);
    line(cx, cy + gap, cx, cy + len);
}

// ============================================================================
// HUD 显示
// ============================================================================

void Renderer::drawHUD(const Camera4D &cam) const
{
    settextcolor(RGB(255, 255, 255));
    setbkmode(TRANSPARENT);

    const Vec4 &pos = cam.getPos();
    const Vec4 &r = cam.getRight();
    const Vec4 &f = cam.getForward();
    const Vec4 &o = cam.getOver();

    wchar_t buf[256];
    HDC hdc = GetImageHDC();

    // 帧率（右上角�?
    swprintf(buf, 256, L"FPS: %d", m_fps);
    TextOutW(hdc, m_screenWidth - 100, 10, buf, (int) wcslen(buf));

    // 坐标
    swprintf(buf, 256, L"Pos: (%.1f, %.1f, %.1f, %.1f)",
        pos.x, pos.y, pos.z, pos.w);
    TextOutW(hdc, 10, 10, buf, (int) wcslen(buf));

    // 基向�?
    swprintf(buf, 256, L"Right:  (%.2f, %.2f, %.2f, %.2f)", r.x, r.y, r.z, r.w);
    TextOutW(hdc, 10, 30, buf, (int) wcslen(buf));

    swprintf(buf, 256, L"Fwd:    (%.2f, %.2f, %.2f, %.2f)", f.x, f.y, f.z, f.w);
    TextOutW(hdc, 10, 50, buf, (int) wcslen(buf));

    swprintf(buf, 256, L"Over:   (%.2f, %.2f, %.2f, %.2f)", o.x, o.y, o.z, o.w);
    TextOutW(hdc, 10, 70, buf, (int) wcslen(buf));

    swprintf(buf, 256, L"Up:     (%.2f, %.2f, %.2f, %.2f)", cam.getUp().x, cam.getUp().y, cam.getUp().z, cam.getUp().w);
    TextOutW(hdc, 10, 90, buf, (int) wcslen(buf));

    // XZW 子空间投影（Y 分量置零�?
    settextcolor(RGB(150, 200, 255));
    swprintf(buf, 256, L"i(fwd_XZW): (%.2f, 0, %.2f, %.2f)", f.x, f.z, f.w);
    TextOutW(hdc, 10, 115, buf, (int) wcslen(buf));
    swprintf(buf, 256, L"j(rgt_XZW): (%.2f, 0, %.2f, %.2f)", r.x, r.z, r.w);
    TextOutW(hdc, 10, 135, buf, (int) wcslen(buf));
    swprintf(buf, 256, L"n(ovr_XZW): (%.2f, 0, %.2f, %.2f)", o.x, o.z, o.w);
    TextOutW(hdc, 10, 155, buf, (int) wcslen(buf));

    // 操作提示
    settextcolor(RGB(200, 200, 200));

    const wchar_t *help1 = L"W/S: Fwd/Back  |  A/D: Strafe  |  Space/Shift: Up/Down(Y)";
    const wchar_t *help2 = L"Mouse H: Yaw  |  Mouse V: Pitch  |  Q/E+Wheel: Slice";
    const wchar_t *help3 = L"I/J/K/L/U/O: PlaneRot  |  LMB: Break  |  RMB: Place  |  R: Reset  |  ESC";

    TextOutW(hdc, 10, m_screenHeight - 80, help1, (int) wcslen(help1));
    TextOutW(hdc, 10, m_screenHeight - 60, help2, (int) wcslen(help2));
    TextOutW(hdc, 10, m_screenHeight - 40, help3, (int) wcslen(help3));
}
