#include "renderer.h"
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cwchar>
#include <windows.h>

// ============================================================================
// 3D 立方体 12 条边（顶点 0-7，汉明距离 1）
// ============================================================================
const int Renderer::CUBE_EDGES[12][2] = {
    {0,1}, {0,2}, {0,4},
    {1,3}, {1,5},
    {2,3}, {2,6},
    {3,7},
    {4,5}, {4,6},
    {5,7},
    {6,7}
};

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
// 生成 3D 立方体的 8 个世界空间顶点（W 固定为方块 W 坐标）
// ============================================================================
static void cubeVerticesWorld(int bx, int by, int bz, int bw, Vec4 verts[8])
{
    double cx = static_cast<double>(bx);
    double cy = static_cast<double>(by);
    double cz = static_cast<double>(bz);
    double cw = static_cast<double>(bw);
    for (int i = 0; i < 8; ++i)
    {
        double sx = (i & 1) ? 0.5 : -0.5;
        double sy = (i & 2) ? 0.5 : -0.5;
        double sz = (i & 4) ? 0.5 : -0.5;
        verts[i] = Vec4(cx + sx, cy + sy, cz + sz, cw);
    }
}

// ============================================================================
// 渲染世界：切片过滤 + 3D 立方体
// ============================================================================

void Renderer::renderWorld(const World &world, const Camera4D &cam)
{
    const auto &blocks = world.getAllBlocks();
    if (blocks.empty()) return;

    Vec4 camPos = cam.getPos();
    const Vec4 &over = cam.getOver();

    // 收集切片内的方块（|overDist| ≤ 0.5 表示切片穿过方块）
    struct BlockInfo { IVec4 pos; double overDist; double depth; };
    std::vector<BlockInfo> visible;
    visible.reserve(blocks.size());

    for (const auto &pair : blocks)
    {
        const IVec4 &p = pair.first;
        Vec4 center(static_cast<double>(p.x),
            static_cast<double>(p.y),
            static_cast<double>(p.z),
            static_cast<double>(p.w));
        Vec4 delta = vec4Sub(center, camPos);
        double od = vec4Dot(delta, over);
        if (od < -0.5 || od > 0.5) continue;  // 切片未穿过此方块

        // 深度用 camZ（forward 方向距离）
        double depth = vec4Dot(delta, cam.getForward());
        visible.push_back({ p, od, depth });
    }

    if (visible.empty()) return;

    // 按深度降序（远到近）
    std::sort(visible.begin(), visible.end(),
        [](const BlockInfo &a, const BlockInfo &b)
    {
        return a.depth > b.depth;
    });

    // 逐个绘制
    for (const auto &bi : visible)
        drawBlock3D(bi.pos, cam, bi.overDist);
}

// ============================================================================
// 绘制单个 3D 立方体
// ============================================================================

void Renderer::drawBlock3D(const IVec4 &blockPos, const Camera4D &cam, double overDist)
{
    // 生成 8 个世界空间顶点（W 固定，X/Y/Z 变化 ±0.5）
    Vec4 worldVerts[8];
    cubeVerticesWorld(blockPos.x, blockPos.y, blockPos.z, blockPos.w, worldVerts);

    // 投影所有顶点：4D→2D
    ProjResult proj[8];
    bool anyValid = false;
    for (int i = 0; i < 8; ++i)
    {
        proj[i] = project(worldVerts[i], cam, m_scale, m_offsetX, m_offsetY, cam.getPitch());
        if (proj[i].valid) anyValid = true;
    }
    if (!anyValid) return;

    // 亮度：离切片越近越亮
    double brightness = 1.0 - std::abs(overDist) * 1.2;
    if (brightness < 0.15) brightness = 0.15;
    if (brightness > 1.0)  brightness = 1.0;
    int gray = static_cast<int>(200 * brightness);
    setlinecolor(RGB(gray, gray, gray));

    // 绘制 12 条边
    for (int e = 0; e < 12; ++e)
    {
        int i0 = CUBE_EDGES[e][0];
        int i1 = CUBE_EDGES[e][1];
        if (!proj[i0].valid || !proj[i1].valid) continue;

        int x0 = static_cast<int>(proj[i0].screenPos.x);
        int y0 = static_cast<int>(proj[i0].screenPos.y);
        int x1 = static_cast<int>(proj[i1].screenPos.x);
        int y1 = static_cast<int>(proj[i1].screenPos.y);
        line(x0, y0, x1, y1);
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
