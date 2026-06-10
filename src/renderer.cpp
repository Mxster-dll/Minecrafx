#include "renderer.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>
#include <cwchar>
#include <windows.h>

// ============================================================================
// 超立方体 32 条边 — 连接汉明距离为 1 的顶点对
// ============================================================================
const int Renderer::EDGE_TABLE[32][2] = {
    {0, 1}, {0, 2}, {0, 4}, {0, 8},
    {1, 3}, {1, 5}, {1, 9},
    {2, 3}, {2, 6}, {2, 10},
    {3, 7}, {3, 11},
    {4, 5}, {4, 6}, {4, 12},
    {5, 7}, {5, 13},
    {6, 7}, {6, 14},
    {7, 15},
    {8, 9}, {8, 10}, {8, 12},
    {9, 11}, {9, 13},
    {10, 11}, {10, 14},
    {11, 15},
    {12, 13}, {12, 14},
    {13, 15},
    {14, 15}
};

// ============================================================================
// 构造与析构
// ============================================================================

Renderer::Renderer(int screenWidth, int screenHeight, double scale)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_scale(scale)
    , m_offsetX(screenWidth / 2.0)
    , m_offsetY(screenHeight / 2.0)
{}

// ============================================================================
// 生成超立方体顶点
// ============================================================================

void Renderer::generateHypercubeVertices(
    int centerX, int centerY, int centerZ, int centerW,
    Vec4 vertices[16])
{
    double cx = static_cast<double>(centerX);
    double cy = static_cast<double>(centerY);
    double cz = static_cast<double>(centerZ);
    double cw = static_cast<double>(centerW);

    for (int i = 0; i < 16; ++i)
    {
        // 位 0→x, 位 1→y, 位 2→z, 位 3→w
        // 位为 0 取 -0.5，位为 1 取 +0.5
        double sx = ((i & 1) ? 0.5 : -0.5);
        double sy = ((i & 2) ? 0.5 : -0.5);
        double sz = ((i & 4) ? 0.5 : -0.5);
        double sw = ((i & 8) ? 0.5 : -0.5);

        vertices[i] = Vec4(cx + sx, cy + sy, cz + sz, cw + sw);
    }
}

// ============================================================================
// 绘制单个方块
// ============================================================================

void Renderer::drawBlock(const IVec4 &blockPos, const Camera4D &cam)
{
    // 生成 16 个世界空间顶点
    Vec4 vertices[16];
    generateHypercubeVertices(blockPos.x, blockPos.y, blockPos.z, blockPos.w, vertices);

    // 投影所有顶点
    ProjResult projected[16];
    bool anyValid = false;
    double avgCamW = 0.0;
    int validCount = 0;

    for (int i = 0; i < 16; ++i)
    {
        projected[i] = project(vertices[i], cam, m_scale, m_offsetX, m_offsetY);
        if (projected[i].valid)
        {
            anyValid = true;
            avgCamW += projected[i].camW;
            ++validCount;
        }
    }

    if (!anyValid)
        return;  // 所有顶点均被裁剪

    // 根据 camW 深度调节亮度（远暗近亮）
    if (validCount > 0)
        avgCamW /= static_cast<double>(validCount);

    // camW 越大越远 → 颜色越暗
    double brightness = 1.0 / (1.0 + avgCamW * 0.3);
    if (brightness > 1.0) brightness = 1.0;
    if (brightness < 0.15) brightness = 0.15;

    // 石块颜色：灰色，根据深度调节
    int r = static_cast<int>(180 * brightness);
    int g = static_cast<int>(180 * brightness);
    int b = static_cast<int>(180 * brightness);
    setlinecolor(RGB(r, g, b));

    // 绘制 32 条边
    for (int e = 0; e < 32; ++e)
    {
        int i0 = EDGE_TABLE[e][0];
        int i1 = EDGE_TABLE[e][1];

        if (!projected[i0].valid || !projected[i1].valid)
            continue;

        int x0 = static_cast<int>(projected[i0].screenPos.x);
        int y0 = static_cast<int>(projected[i0].screenPos.y);
        int x1 = static_cast<int>(projected[i1].screenPos.x);
        int y1 = static_cast<int>(projected[i1].screenPos.y);

        line(x0, y0, x1, y1);
    }
}

// ============================================================================
// 渲染世界
// ============================================================================

void Renderer::renderWorld(const World &world, const Camera4D &cam)
{
    const auto &blocks = world.getAllBlocks();
    if (blocks.empty())
        return;

    // 收集所有方块并计算到摄像机的平方距离
    struct BlockDist
    {
        IVec4 pos;
        double distSq;
    };

    std::vector<BlockDist> blockList;
    blockList.reserve(blocks.size());

    Vec4 camPos = cam.getPos();
    for (const auto &pair : blocks)
    {
        const IVec4 &pos = pair.first;
        // 方块中心世界坐标
        Vec4 center(static_cast<double>(pos.x),
            static_cast<double>(pos.y),
            static_cast<double>(pos.z),
            static_cast<double>(pos.w));
        double dSq = vec4DistSq(center, camPos);
        blockList.push_back({ pos, dSq });
    }

    // 按距离降序排序（从远到近绘制，减轻重叠混乱）
    std::sort(blockList.begin(), blockList.end(),
        [](const BlockDist &a, const BlockDist &b)
    {
        return a.distSq > b.distSq;
    });

    // 逐个绘制
    for (const auto &bd : blockList)
    {
        drawBlock(bd.pos, cam);
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

    // 操作提示
    settextcolor(RGB(200, 200, 200));

    const wchar_t *help1 = L"W/S/A/D: Move  |  Space/Shift: Up/Down  |  Q/E: 4D strafe";
    const wchar_t *help2 = L"1-6: Rotate planes  |  LMB: Break  |  RMB: Place  |  R: Reset";
    const wchar_t *help3 = L"ESC: Exit";

    TextOutW(hdc, 10, m_screenHeight - 80, help1, (int) wcslen(help1));
    TextOutW(hdc, 10, m_screenHeight - 60, help2, (int) wcslen(help2));
    TextOutW(hdc, 10, m_screenHeight - 40, help3, (int) wcslen(help3));
}
