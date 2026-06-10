#include "renderer.h"
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cwchar>
#include <windows.h>

// ============================================================================
// 超立方体 24 个二维面（每个面 4 个顶点，构成一个正方形）
// ============================================================================
const int Renderer::FACES[24][4] = {
    // XY 面（z,w 固定，变化 bit0,bit1：顺序 0-1-3-2）
    {0,1,3,2}, {4,5,7,6}, {8,9,11,10}, {12,13,15,14},
    // XZ 面（y,w 固定，变化 bit0,bit2：顺序 0-1-5-4）
    {0,1,5,4}, {2,3,7,6}, {8,9,13,12}, {10,11,15,14},
    // XW 面（y,z 固定，变化 bit0,bit3：顺序 0-2-6-4）
    {0,2,6,4}, {1,3,7,5}, {8,10,14,12}, {9,11,15,13},
    // YZ 面（x,w 固定，变化 bit1,bit2：顺序 0-2,3,1  → 0-2-10-8? 不对，变化 bit1,bit2）
    // 顶点: 0(0000), 2(0100), 10(1010), 8(1000) → 顺序 0-2-10-8
    {0,2,10,8}, {1,3,11,9}, {4,6,14,12}, {5,7,15,13},
    // YW 面（x,z 固定，变化 bit1,bit3：顺序 0-1-9-8）
    {0,1,9,8}, {2,3,11,10}, {4,5,13,12}, {6,7,15,14},
    // ZW 面（x,y 固定，变化 bit2,bit3：顺序 0-4-12-8）
    {0,4,12,8}, {1,5,13,9}, {2,6,14,10}, {3,7,15,11}
};

// 生成 16 个顶点
static void hypercubeVertices(int bx, int by, int bz, int bw, Vec4 v[16])
{
    double cx = static_cast<double>(bx);
    double cy = static_cast<double>(by);
    double cz = static_cast<double>(bz);
    double cw = static_cast<double>(bw);
    for (int i = 0; i < 16; ++i)
    {
        double sx = (i & 1) ? 0.5 : -0.5;
        double sy = (i & 2) ? 0.5 : -0.5;
        double sz = (i & 4) ? 0.5 : -0.5;
        double sw = (i & 8) ? 0.5 : -0.5;
        v[i] = Vec4(cx + sx, cy + sy, cz + sz, cw + sw);
    }
}

// ============================================================================
// 构造
// ============================================================================

Renderer::Renderer(int screenWidth, int screenHeight, double scale)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_scale(scale)
    , m_offsetX(screenWidth / 2.0)
    , m_offsetY(screenHeight / 2.0)
{}

// ============================================================================
// 渲染世界
// ============================================================================

void Renderer::renderWorld(const World &world, const Camera4D &cam)
{
    const auto &blocks = world.getAllBlocks();
    if (blocks.empty()) return;

    Vec4 camPos = cam.getPos();

    struct BlockInfo { int x, y, z, w; double depth; };
    std::vector<BlockInfo> visible;
    visible.reserve(blocks.size());

    for (const auto &pair : blocks)
    {
        const IVec4 &p = pair.first;
        Vec4 center(static_cast<double>(p.x), static_cast<double>(p.y),
            static_cast<double>(p.z), static_cast<double>(p.w));
        Vec4 delta = vec4Sub(center, camPos);
        double depth = vec4Dot(delta, cam.getForward());
        visible.push_back({ p.x, p.y, p.z, p.w, depth });
    }

    // 按深度降序
    std::sort(visible.begin(), visible.end(),
        [](const BlockInfo &a, const BlockInfo &b)
    {
        return a.depth > b.depth;
    });

    for (const auto &bi : visible)
        drawBlockSlice(bi.x, bi.y, bi.z, bi.w, cam, world);
}

// ============================================================================
// 超平面截 4D 立方体：真实交线
// ============================================================================

void Renderer::drawBlockSlice(int bx, int by, int bz, int bw,
    const Camera4D &cam, const World &world)
{
    // 可见性测试：检查 8 个方向是否都被方块紧邻
    bool occluded =
        world.get(IVec4(bx + 1, by, bz, bw)) != 0 &&
        world.get(IVec4(bx - 1, by, bz, bw)) != 0 &&
        world.get(IVec4(bx, by + 1, bz, bw)) != 0 &&
        world.get(IVec4(bx, by - 1, bz, bw)) != 0 &&
        world.get(IVec4(bx, by, bz + 1, bw)) != 0 &&
        world.get(IVec4(bx, by, bz - 1, bw)) != 0 &&
        world.get(IVec4(bx, by, bz, bw + 1)) != 0 &&
        world.get(IVec4(bx, by, bz, bw - 1)) != 0;

    if (occluded)
        setlinecolor(RGB(220, 40, 40));  // 红色标记：全遮挡
    else
        setlinecolor(RGB(180, 180, 180));

    // 生成 16 个顶点
    Vec4 worldVerts[16];
    hypercubeVertices(bx, by, bz, bw, worldVerts);

    // 计算每个顶点到切片的 over 距离
    double overDist[16];
    Vec4 camPos = cam.getPos();
    const Vec4 &over = cam.getOver();
    for (int i = 0; i < 16; ++i)
        overDist[i] = vec4Dot(vec4Sub(worldVerts[i], camPos), over);

    // 遍历 24 个面
    for (int f = 0; f < 24; ++f)
    {
        const int *face = FACES[f];

        // 收集此面内与切片相交的边
        Vec4 hitPoints[4];
        int hitCount = 0;

        // 检查 4 条边（正方形中相邻顶点对）
        for (int e = 0; e < 4; ++e)
        {
            int a = face[e];
            int b = face[(e + 1) & 3];  // 循环相邻
            double da = overDist[a];
            double db = overDist[b];

            if ((da > 0 && db > 0) || (da < 0 && db < 0))
                continue;  // 同侧，不相交

            // 计算交点（线性插值）
            double t = 0.0;
            if (std::abs(da - db) > 1e-12)
                t = da / (da - db);
            else
                t = 0.5;

            Vec4 pt = vec4Add(worldVerts[a],
                vec4Scale(vec4Sub(worldVerts[b], worldVerts[a]), t));
            hitPoints[hitCount++] = pt;
            if (hitCount >= 4) break;
        }

        // 绘制交线段
        for (int i = 0; i + 1 < hitCount; i += 2)
        {
            ProjResult p0 = project(hitPoints[i], cam, m_scale, m_offsetX, m_offsetY, cam.getPitch());
            ProjResult p1 = project(hitPoints[i + 1], cam, m_scale, m_offsetX, m_offsetY, cam.getPitch());
            if (p0.valid && p1.valid)
                line(static_cast<int>(p0.screenPos.x), static_cast<int>(p0.screenPos.y),
                    static_cast<int>(p1.screenPos.x), static_cast<int>(p1.screenPos.y));
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
    // 水平线
    line(cx - len, cy, cx - gap, cy);
    line(cx + gap, cy, cx + len, cy);
    // 垂直线
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

    // 坐标
    swprintf(buf, 256, L"Pos: (%.1f, %.1f, %.1f, %.1f)",
        pos.x, pos.y, pos.z, pos.w);
    TextOutW(hdc, 10, 10, buf, (int) wcslen(buf));

    // 基向量
    swprintf(buf, 256, L"Right:  (%.2f, %.2f, %.2f, %.2f)", r.x, r.y, r.z, r.w);
    TextOutW(hdc, 10, 30, buf, (int) wcslen(buf));

    swprintf(buf, 256, L"Fwd:    (%.2f, %.2f, %.2f, %.2f)", f.x, f.y, f.z, f.w);
    TextOutW(hdc, 10, 50, buf, (int) wcslen(buf));

    swprintf(buf, 256, L"Over:   (%.2f, %.2f, %.2f, %.2f)", o.x, o.y, o.z, o.w);
    TextOutW(hdc, 10, 70, buf, (int) wcslen(buf));

    swprintf(buf, 256, L"Up:     (%.2f, %.2f, %.2f, %.2f)", cam.getUp().x, cam.getUp().y, cam.getUp().z, cam.getUp().w);
    TextOutW(hdc, 10, 90, buf, (int) wcslen(buf));

    // XZW 子空间投影（Y 分量置零）
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
