#include "renderer.h"
#include <algorithm>
#include <vector>
#include <utility>
#include <cstdio>
#include <cwchar>
#include <functional>
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

// 预计算：每个面哪两 bit 固定及其值（位掩码）
static const auto s_fix = []()
{
    struct { int bits[24]; int vals[24]; } r;
    for (int f = 0; f < 24; ++f)
    {
        const int *fc = Renderer::FACES[f];
        int v0 = fc[0], bm = 0, vm = 0;
        for (int bit = 0; bit < 4; ++bit)
        {
            int m = 1 << bit; bool same = true;
            for (int i = 1; i < 4; ++i) if ((fc[i] & m) != (v0 & m)) { same = false; break; }
            if (same) { bm |= m; if (v0 & m) vm |= m; }
        }
        r.bits[f] = bm; r.vals[f] = vm;
    }
    return r;
}();

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
    , m_diagTotal(0)
    , m_diagSlice(0)
    , m_diagOccl(0)
    , m_diagGeom(0)
    , m_diagFaces(0)
    , m_timeZBuf(0)
    , m_timeDIB(0)
    , m_timeCellTest(0)
    , m_timeSurfChk(0)
    , m_timeVertGen(0)
    , m_timeOverDot(0)
    , m_time24Face(0)
    , m_timeCellGrp(0)
    , m_timeEpiMatch(0)
    , m_timeChain(0)
    , m_timeDSort(0)
    , m_timeSort(0)
    , m_timeBBOX(0)
    , m_timeEdges(0)
    , m_timePixWr(0)
    , m_timeBitBlt(0)
    , m_timeWorld(0)
    , m_timeElapsed(0)
    , m_tPrev(0)
    , m_timeSamples(0)
    , m_sliceStart(0)
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
    clock_t tw0 = clock();
    ++m_frameCount;

    clock_t t = clock();
    resetBuffers();
    m_timeZBuf += clock() - t;  t = clock();

    HDC hdc = GetImageHDC();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_screenWidth;
    bmi.bmiHeader.biHeight = -m_screenHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    DWORD *bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
        reinterpret_cast<void **>(&bits), nullptr, 0);
    if (!hBmp || !bits) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP oldBmp = (HBITMAP) SelectObject(memDC, hBmp);

    DWORD bg = 0x001E0A0A;
    int total = m_screenWidth * m_screenHeight;
    for (int i = 0; i < total; ++i) bits[i] = bg;
    m_timeDIB += clock() - t;  t = clock();

    m_pBits = bits;
    drawFacesStep(world, cam);
    m_pBits = nullptr;

    clock_t tBlt = clock();
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, memDC, 0, 0, SRCCOPY);
    m_timeBitBlt += clock() - tBlt;

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

    m_timeWorld += clock() - tw0;

    // 帧间实际耗时
    if (m_tPrev != 0)
        m_timeElapsed += clock() - m_tPrev;
    m_tPrev = clock();

    // 100ms 时间切片：当前切片满则推入队列，弹出最旧切片
    if (m_sliceStart == 0) m_sliceStart = clock();  // 首帧初始化
    if (clock() - m_sliceStart >= SLICE_TICKS)
    {
        TimeSlice ts;
        ts.zBuf = m_timeZBuf;       ts.dib = m_timeDIB;
        ts.cellTest = m_timeCellTest; ts.surfChk = m_timeSurfChk;
        ts.vertGen = m_timeVertGen;   ts.overDot = m_timeOverDot;
        ts.f24 = m_time24Face;        ts.cellGrp = m_timeCellGrp;
        ts.epiMatch = m_timeEpiMatch; ts.chain = m_timeChain;
        ts.dsort = m_timeDSort;       ts.sort_ = m_timeSort;
        ts.bbox = m_timeBBOX;         ts.edges = m_timeEdges;
        ts.pixWr = m_timePixWr;       ts.bitBlt = m_timeBitBlt;
        ts.world = m_timeWorld;       ts.elapsed = m_timeElapsed;
        ts.samples = m_timeSamples;
        m_timeSlices.push_back(ts);
        if (static_cast<int>(m_timeSlices.size()) > TIME_SLICES)
            m_timeSlices.pop_front();
        // 重置当前切片
        m_timeZBuf = 0;   m_timeDIB = 0;      m_timeCellTest = 0;
        m_timeSurfChk = 0; m_timeVertGen = 0;  m_timeOverDot = 0;
        m_time24Face = 0;  m_timeCellGrp = 0;  m_timeEpiMatch = 0;
        m_timeChain = 0;   m_timeDSort = 0;    m_timeSort = 0;
        m_timeBBOX = 0;    m_timeEdges = 0;    m_timePixWr = 0;
        m_timeBitBlt = 0;  m_timeWorld = 0;    m_timeElapsed = 0;
        m_timeSamples = 0;
        m_sliceStart = clock();
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

    Vec4 camPos = cam.getPos();
    const Vec4 &ov = cam.getOver();

    m_diagTotal = static_cast<int>(blocks.size())
        + static_cast<int>(m_superBlocks.size()) * SuperBlock::SIZE * SuperBlock::SIZE
        * SuperBlock::SIZE * SuperBlock::SIZE;
    if (m_diagTotal == 0) return;
    m_diagSlice = 0;
    m_diagOccl = 0;
    m_diagGeom = 0;
    m_diagFaces = 0;
    double sp = m_blockHalf * 2.0;

    struct FaceData { int bx, by, bz, bw; COLORREF col; POINT pts[12]; double depths[12]; int n; };
    std::vector<FaceData> allFaces;

    clock_t tOcclAcc = 0;

    for (const auto &pair : blocks)
    {
        int bx = pair.first.x, by = pair.first.y, bz = pair.first.z, bw = pair.first.w;
        if (!mayIntersectSlice(bx, by, bz, bw, camPos, ov, m_blockHalf, m_blockHalf * 2.0))
            continue;
        ++m_diagSlice;

        clock_t tOccl0 = clock();
        if (world.get(IVec4(bx + 1, by, bz, bw)) && world.get(IVec4(bx - 1, by, bz, bw)) &&
            world.get(IVec4(bx, by + 1, bz, bw)) && world.get(IVec4(bx, by - 1, bz, bw)) &&
            world.get(IVec4(bx, by, bz + 1, bw)) && world.get(IVec4(bx, by, bz - 1, bw)) &&
            world.get(IVec4(bx, by, bz, bw + 1)) && world.get(IVec4(bx, by, bz, bw - 1)))
        {
            tOcclAcc += (clock() - tOccl0);
            continue;
        }
        tOcclAcc += (clock() - tOccl0);
        ++m_diagOccl;

        clock_t tG = clock();
        COLORREF col = getBlockColor(bx, by, bz, bw);

        Vec4 verts[16]; hypercubeVertices(bx, by, bz, bw, verts, m_blockHalf);
        m_timeVertGen += clock() - tG;  tG = clock();

        double od[16];
        for (int i = 0; i < 16; ++i)
            od[i] = vec4Dot(vec4Sub(verts[i], camPos), ov);
        m_timeOverDot += clock() - tG;  tG = clock();

        // 收集交线
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
                double t2 = (std::abs(da - db) > 1e-12) ? da / (da - db) : 0.5;
                hits[hc++] = vec4Add(verts[a], vec4Scale(vec4Sub(verts[b], verts[a]), t2));
            }
            if (hc == 2) segs[sc++] = { hits[0], hits[1], f };
        }
        if (sc == 0) { m_time24Face += clock() - tG; continue; }
        ++m_diagGeom;
        m_time24Face += clock() - tG;  tG = clock();

        // 8 个胞腔
        struct Cell { int bit, val; };
        const Cell cells[8] = { {0,0},{0,1},{1,0},{1,1},{2,0},{2,1},{3,0},{3,1} };

        for (int ci = 0; ci < 8; ++ci)
        {
            int cb = cells[ci].bit, cv = cells[ci].val;
            int cSegs[6]; int csc = 0;
            for (int s = 0; s < sc; ++s)
            {
                int fi = segs[s].faceIdx;
                int bm = s_fix.bits[fi];
                if ((bm >> cb) & 1)
                {  // bit cb 在此面中固定
                    if (((s_fix.vals[fi] >> cb) & 1) == cv)  // 固定值等于胞腔值
                        if (csc < 6) cSegs[csc++] = s;
                }
            }
            if (csc < 3) continue;

            m_timeCellGrp += clock() - tG;  tG = clock();

            // ---- 通过线段端点匹配构建正确的多边形顶点顺序 ----
            struct EP { Vec4 pos; int segIdx; };
            EP eps[12]; int epCount = 0;
            for (int i = 0; i < csc; ++i)
            {
                int si = cSegs[i];
                eps[epCount++] = { segs[si].a, i };
                eps[epCount++] = { segs[si].b, i };
            }

            int next[12];
            for (int i = 0; i < epCount; ++i)
            {
                next[i] = -1;
                for (int j = 0; j < epCount; ++j)
                {
                    if (i == j) continue;
                    if (eps[i].segIdx == eps[j].segIdx) continue;
                    if (vec4DistSq(eps[i].pos, eps[j].pos) < 1e-6)
                    {
                        int other = -1;
                        for (int k = 0; k < epCount; ++k)
                            if (k != j && eps[k].segIdx == eps[j].segIdx) { other = k; break; }
                        next[i] = other;
                        break;
                    }
                }
            }
            m_timeEpiMatch += clock() - tG;  tG = clock();

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
            m_timeChain += clock() - tG;  tG = clock();

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

    // 累加哈希表路径的遮挡检测耗时
    m_timeSurfChk += tOcclAcc;

    // ---- 超方块：16 分法递归遍历 ----
    for (const auto &sb : m_superBlocks)
    {
        int baseX = sb.pos().x * SuperBlock::SIZE;
        int baseY = sb.pos().y * SuperBlock::SIZE;
        int baseZ = sb.pos().z * SuperBlock::SIZE;
        int baseW = sb.pos().w * SuperBlock::SIZE;

        std::function<void(int, int, int, int, int)> traverse =
            [&](int bx, int by, int bz, int bw, int size)
        {
            clock_t t = clock();
            // 胞腔-切片相交测试
            double cs = size * sp;
            double cx = (bx + size * 0.5) * sp;
            double cy = (by + size * 0.5) * sp;
            double cz = (bz + size * 0.5) * sp;
            double cw = (bw + size * 0.5) * sp;
            double cod = ov.x * (cx - camPos.x) + ov.y * (cy - camPos.y) + ov.z * (cz - camPos.z) + ov.w * (cw - camPos.w);
            double ext = cs * (std::abs(ov.x) + std::abs(ov.y) + std::abs(ov.z) + std::abs(ov.w));
            if (std::abs(cod) > ext + 1e-9) return;
            ++m_diagSlice;
            m_timeCellTest += clock() - t;  t = clock();

            if (size == 1)
            {
                int lx = bx - baseX, ly = by - baseY, lz = bz - baseZ, lw = bw - baseW;
                if (lx > 0 && lx < SuperBlock::SIZE - 1 && ly>0 && ly < SuperBlock::SIZE - 1 &&
                    lz>0 && lz < SuperBlock::SIZE - 1 && lw>0 && lw < SuperBlock::SIZE - 1) return;
                ++m_diagOccl;
                m_timeSurfChk += clock() - t;  t = clock();

                COLORREF col = getBlockColor(bx, by, bz, bw);
                Vec4 verts[16]; hypercubeVertices(bx, by, bz, bw, verts, m_blockHalf);
                m_timeVertGen += clock() - t;  t = clock();

                double od[16];
                for (int i = 0; i < 16; ++i) od[i] = vec4Dot(vec4Sub(verts[i], camPos), ov);
                m_timeOverDot += clock() - t;  t = clock();

                struct Seg2 { Vec4 a, b; int fi; };
                Seg2 segs[24]; int sc = 0;
                for (int f = 0; f < 24; ++f)
                {
                    const int *fc = FACES[f]; Vec4 h[4]; int hc = 0;
                    for (int e = 0; e < 4 && hc < 4; ++e)
                    {
                        int a = fc[e], b = fc[(e + 1) & 3];
                        double da = od[a], db = od[b];
                        if ((da > 0 && db > 0) || (da < 0 && db < 0)) continue;
                        double t2 = (std::abs(da - db) > 1e-12) ? da / (da - db) : 0.5;
                        h[hc++] = vec4Add(verts[a], vec4Scale(vec4Sub(verts[b], verts[a]), t2));
                    }
                    if (hc == 2) segs[sc++] = { h[0],h[1],f };
                }
                if (sc == 0) return;
                ++m_diagGeom;
                m_time24Face += clock() - t;  t = clock();

                struct Cell { int bit, val; };
                const Cell cells[8] = { {0,0},{0,1},{1,0},{1,1},{2,0},{2,1},{3,0},{3,1} };
                for (int ci = 0; ci < 8; ++ci)
                {
                    int cb = cells[ci].bit, cv = cells[ci].val;
                    int cSegs[6]; int csc = 0;
                    for (int s = 0; s < sc; ++s)
                    {
                        int fi = segs[s].fi;
                        int bm = s_fix.bits[fi];
                        if ((bm >> cb) & 1)
                        {
                            if (((s_fix.vals[fi] >> cb) & 1) == cv)
                                if (csc < 6) cSegs[csc++] = s;
                        }
                    }
                    if (csc < 3) continue;
                    struct EP { Vec4 pos; int si; int pt; };
                    EP eps[12]; int epc = 0;
                    for (int i = 0; i < csc; ++i)
                    {
                        int si = cSegs[i]; int ai = epc;
                        eps[epc++] = { segs[si].a,i,ai + 1 }; eps[epc++] = { segs[si].b,i,ai };
                    }
                    m_timeCellGrp += clock() - t;  t = clock();

                    int next[12];
                    for (int i = 0; i < epc; ++i)
                    {
                        next[i] = -1;
                        for (int j = 0; j < epc; ++j)
                        {
                            if (eps[i].si == eps[j].si) continue;
                            if (vec4DistSq(eps[i].pos, eps[j].pos) < 1e-6) { next[i] = eps[j].pt; break; }
                        }
                    }
                    m_timeEpiMatch += clock() - t;  t = clock();

                    POINT oPt[12]; double oDp[12]; int oN = 0; bool used[12] = {}; int cur = 0;
                    while (cur >= 0 && !used[cur] && oN < 12)
                    {
                        used[cur] = true;
                        ProjResult pr = project(eps[cur].pos, cam, m_scale, m_offsetX, m_offsetY, cam.getPitch());
                        if (!pr.valid) break;
                        oPt[oN] = { (int) pr.screenPos.x,(int) pr.screenPos.y };
                        oDp[oN] = vec4Dot(vec4Sub(eps[cur].pos, camPos), cam.getForward());
                        ++oN; cur = next[cur];
                    }
                    m_timeChain += clock() - t;  t = clock();
                    if (oN < 3) continue;
                    FaceData fd = { bx,by,bz,bw,col,{},{},0 };
                    for (int i = 0; i < oN && fd.n < 12; ++i) { fd.pts[fd.n] = oPt[i]; fd.depths[fd.n] = oDp[i]; ++fd.n; }
                    allFaces.push_back(fd);
                }
                return;
            }
            int h = size / 2;
            for (int dx = 0; dx < 2; ++dx) for (int dy = 0; dy < 2; ++dy)
                for (int dz = 0; dz < 2; ++dz) for (int dw = 0; dw < 2; ++dw)
                    traverse(bx + dx * h, by + dy * h, bz + dz * h, bw + dw * h, h);
        };
        traverse(baseX, baseY, baseZ, baseW, SuperBlock::SIZE);
    }

    m_diagFaces = static_cast<int>(allFaces.size());

    // 深度汇总
    // clock_t tSumZ = clock();
    // struct FaceWithDepth { FaceData fd; double avgDepth; };
    // std::vector<FaceWithDepth> fds;
    // for (auto &fd : allFaces)
    // {
    //     double sumZ = 0;
    //     for (int i = 0; i < fd.n; ++i) sumZ += fd.depths[i];
    //     fds.push_back({ fd, sumZ / fd.n });
    // }
    // m_timeDSort += clock() - tSumZ;  // 汇总

    // 排序
    // clock_t tSort = clock();
    // std::sort(fds.begin(), fds.end(), [](const FaceWithDepth &a, const FaceWithDepth &b)
    // {
    //     return a.avgDepth > b.avgDepth;
    // });
    // m_timeSort += clock() - tSort;

    // 扫描线填充（不排序，直接按收集顺序绘制）
    clock_t tFill0 = clock();
    int showCount = static_cast<int>(allFaces.size());
    for (int i = 0; i < static_cast<int>(allFaces.size()) && i < showCount; ++i)
    {
        const FaceData &fd = allFaces[i];
        int r = GetRValue(fd.col), g = GetGValue(fd.col), b = GetBValue(fd.col);
        fillPolygonZ(fd.pts, fd.n, fd.depths, RGB(r, g, b));
    }
    m_timePixWr += clock() - tFill0;
    ++m_timeSamples;
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

    // ---- 预计算每条边的斜率（除法只做一次） ----
    struct EdgePre { double x, z; double dxdy, dzdy; int yStart; int yEnd; };
    EdgePre epre[12]; int ec = 0;
    double *zbuf = m_zbuf.data();
    DWORD *bits = m_pBits;
    int sw = m_screenWidth;

    for (int i = 0; i < n; ++i)
    {
        int j = (i + 1) % n;
        int y0 = pts[i].y, y1 = pts[j].y;
        if (y0 == y1) continue;  // 水平边跳过

        // 确保 y0 < y1（从顶到底）
        int topY, botY, topIdx;
        if (y0 < y1) { topY = y0; botY = y1; topIdx = i; }
        else { topY = y1; botY = y0; topIdx = j; }

        int otherIdx = (topIdx == i) ? j : i;
        double invDy = 1.0 / (botY - topY);
        double xTop = pts[topIdx].x;
        double zTop = depths[topIdx];
        double dx = pts[otherIdx].x - xTop;
        double dz = depths[otherIdx] - zTop;
        double dxdy = dx * invDy;
        double dzdy = dz * invDy;

        // 推进到第一条有效扫描线
        int startY = topY;
        if (startY < minY)
        {
            double adv = static_cast<double>(minY - topY);
            xTop += dxdy * adv;
            zTop += dzdy * adv;
            startY = minY;
        }
        if (startY > maxY) continue;

        int yEnd = botY;
        if (yEnd > maxY + 1) yEnd = maxY + 1;

        epre[ec++] = { xTop, zTop, dxdy, dzdy, startY, yEnd };
    }

    // ---- 扫描线循环（凸多边形：每条线恰有 0/2 交点） ----
    for (int y = minY; y <= maxY; ++y)
    {
        double xL = 1e9, xR = -1e9, zL = 0, zR = 0;
        int hit = 0;

        for (int e = 0; e < ec; ++e)
        {
            if (y < epre[e].yStart || y >= epre[e].yEnd) continue;
            double cx = epre[e].x, cz = epre[e].z;
            if (cx < xL) { xL = cx; zL = cz; }
            if (cx > xR) { xR = cx; zR = cz; }
            ++hit;
            epre[e].x += epre[e].dxdy;
            epre[e].z += epre[e].dzdy;
        }

        if (hit < 2) continue;

        int x0 = static_cast<int>(xL), x1 = static_cast<int>(xR);
        if (x0 < 0) x0 = 0;
        if (x1 >= sw) x1 = sw - 1;
        if (x0 > x1) continue;

        double dz = (x1 > x0) ? (zR - zL) / (x1 - x0) : 0;
        double z = zL;
        double *zptr = zbuf + y * sw + x0;
        DWORD *bptr = bits + y * sw + x0;
        for (int x = x0; x <= x1; ++x, z += dz, ++zptr, ++bptr)
        {
            if (z < *zptr) { *zptr = z; *bptr = color; }
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
    line(cx - len, cy, cx - gap, cy);
    line(cx + gap, cy, cx + len, cy);
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
        // 斜二侧画法：WZ 平面真实比例，X 轴 45° 左下，半深
        const double k = 0.35355339;  // 0.5 × cos(45°)
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

    // 找平面内两个正交方向 u, v（与 n3 垂直）
    Vec4 u3, v3;
    // 挑一个不平行于 n3 的向量
    if (std::abs(n3.x) < 0.9)      u3 = Vec4(1.0, 0.0, 0.0, 0.0);
    else if (std::abs(n3.z) < 0.9) u3 = Vec4(0.0, 0.0, 1.0, 0.0);
    else                           u3 = Vec4(0.0, 0.0, 0.0, 1.0);
    // u = normalize(u3 - (u3·n3)*n3)
    double dotUN = u3.x * n3.x + u3.z * n3.z + u3.w * n3.w;
    u3 = Vec4(u3.x - dotUN * n3.x, 0.0, u3.z - dotUN * n3.z, u3.w - dotUN * n3.w);
    double uLen = std::sqrt(u3.x * u3.x + u3.z * u3.z + u3.w * u3.w);
    if (uLen > 1e-9) { u3.x /= uLen; u3.z /= uLen; u3.w /= uLen; }
    // v = n3 × u3（3D 叉积在 XZW 空间）
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
    double invCLK = 1000.0 / CLOCKS_PER_SEC;

    // 汇总所有切片 + 当前累加器
    clock_t sumZBuf = m_timeZBuf, sumDIB = m_timeDIB, sumCellT = m_timeCellTest;
    clock_t sumSurf = m_timeSurfChk, sumVertG = m_timeVertGen, sumOverD = m_timeOverDot;
    clock_t sum24F = m_time24Face, sumCellG = m_timeCellGrp, sumEpiM = m_timeEpiMatch;
    clock_t sumChain = m_timeChain, sumDSort = m_timeDSort, sumSort = m_timeSort;
    clock_t sumBBOX = m_timeBBOX, sumEdges = m_timeEdges, sumPixWr = m_timePixWr;
    clock_t sumBlt = m_timeBitBlt, sumWorld = m_timeWorld, sumElapsed = m_timeElapsed;
    int totalSamples = m_timeSamples;
    for (const auto &ts : m_timeSlices)
    {
        sumZBuf += ts.zBuf;   sumDIB += ts.dib;     sumCellT += ts.cellTest;
        sumSurf += ts.surfChk; sumVertG += ts.vertGen; sumOverD += ts.overDot;
        sum24F += ts.f24;     sumCellG += ts.cellGrp; sumEpiM += ts.epiMatch;
        sumChain += ts.chain; sumDSort += ts.dsort;   sumSort += ts.sort_;
        sumBBOX += ts.bbox;   sumEdges += ts.edges;   sumPixWr += ts.pixWr;
        sumBlt += ts.bitBlt;  sumWorld += ts.world;   sumElapsed += ts.elapsed;
        totalSamples += ts.samples;
    }

    auto ms = [&](clock_t t) { return (totalSamples > 0) ? (t * invCLK / totalSamples) : 0.0; };
    double mZBuf = ms(sumZBuf);
    double mDIB = ms(sumDIB);
    double mCellT = ms(sumCellT);
    double mSurf = ms(sumSurf);
    double mVertG = ms(sumVertG);
    double mOverD = ms(sumOverD);
    double m24F = ms(sum24F);
    double mCellG = ms(sumCellG);
    double mEpiM = ms(sumEpiM);
    double mChain = ms(sumChain);
    double mDSort = ms(sumDSort);    double mSort = ms(sumSort);    double mBBOX = ms(sumBBOX);
    double mEdges = ms(sumEdges);
    double mPixWr = ms(sumPixWr);
    double mBlt = ms(sumBlt);
    double mWorld = ms(sumWorld);
    double mElapsed = ms(sumElapsed);
    double mGeom = mVertG + mOverD + m24F + mCellG + mEpiM + mChain;
    double mRast = mDSort + mSort + mBBOX + mEdges + mPixWr;
    double mTotal = mZBuf + mDIB + mCellT + mSurf + mGeom + mRast + mBlt;
    double mOther = mElapsed - mWorld;
    if (mOther < 0.0) mOther = 0.0;

    int xLeft = 530;
    int xRight = m_screenWidth - 12;

    settextcolor(RGB(255, 255, 255));
    SetTextAlign(hdc, TA_LEFT);
    swprintf(buf, 256, L"FPS: %d (%.0fms)", m_fps, mElapsed);
    TextOutW(hdc, xLeft, 10, buf, (int) wcslen(buf));
    SetTextAlign(hdc, TA_RIGHT);
    swprintf(buf, 256, L"子项计: %5.1fms", mTotal);
    TextOutW(hdc, xRight, 10, buf, (int) wcslen(buf));

    auto drawRow = [&](int y, const wchar_t *label, double val, COLORREF clr)
    {
        settextcolor(clr);
        SetTextAlign(hdc, TA_LEFT);
        swprintf(buf, 256, L"%ls", label);  TextOutW(hdc, xLeft, y, buf, (int) wcslen(buf));
        SetTextAlign(hdc, TA_RIGHT);
        swprintf(buf, 256, L"%5.1fms", val); TextOutW(hdc, xRight, y, buf, (int) wcslen(buf));
    };

    COLORREF Y = RGB(255, 255, 100), D = RGB(200, 200, 120);
    drawRow(30, L"清空深度缓冲:", mZBuf, Y);
    drawRow(48, L"创建DIB+背景:", mDIB, Y);
    drawRow(66, L"16分法相交测试:", mCellT, Y);
    drawRow(84, L"表面方块判断:", mSurf, Y);
    drawRow(102, L"生成16顶点:", mVertG, D);
    drawRow(118, L"16次over点积:", mOverD, D);
    drawRow(134, L"24面边求交:", m24F, D);
    drawRow(150, L"胞腔分组:", mCellG, D);
    drawRow(166, L"端点匹配(next[]):", mEpiM, D);
    drawRow(182, L"链追踪+投影:", mChain, D);
    // drawRow(200, L"深度汇总:", mDSort, Y);
    // drawRow(218, L"排序:", mSort, Y);
    drawRow(236, L"填充-逐像素写:", mPixWr, Y);
    drawRow(254, L"BitBlt刷屏:", mBlt, Y);
    drawRow(272, L"渲染总计:", mWorld, Y);
    drawRow(290, L"其他:", mOther, RGB(255, 150, 100));

    // 方块总数 + 面数
    settextcolor(Y);
    SetTextAlign(hdc, TA_LEFT);
    swprintf(buf, 256, L"方块总数: %d", m_diagTotal);
    TextOutW(hdc, xLeft, 320, buf, (int) wcslen(buf));
    SetTextAlign(hdc, TA_RIGHT);
    swprintf(buf, 256, L"面:%d", m_diagFaces);
    TextOutW(hdc, xRight, 320, buf, (int) wcslen(buf));

    SetTextAlign(hdc, TA_LEFT);
    settextcolor(RGB(255, 255, 255));

    // ========================================
    // 坐标 + 俯仰角（左侧，坐标系下方）
    // ========================================
    int infoY = vpY + vpH + 5;
    swprintf(buf, 256, L"Pos: (%.1f, %.1f, %.1f, %.1f)",
        pos.x, pos.y, pos.z, pos.w);
    TextOutW(hdc, 10, infoY, buf, (int) wcslen(buf));
    infoY += 18;

    double pitchDeg = cam.getPitch() * 180.0 / 3.1415926535;
    swprintf(buf, 256, L"俯仰: %+.0f°", pitchDeg);
    TextOutW(hdc, 10, infoY, buf, (int) wcslen(buf));
}
