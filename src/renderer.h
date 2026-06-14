#pragma once

#include "world.h"
#include "camera.h"
#include "linalg.h"
#include "superblock.h"
#include "project4d.h"
#include <graphics.h>
#include <vector>
#include <deque>
#include <unordered_set>
#include <ctime>

/**
 * @brief 4D→3D→2D 渲染器
 *
 * 管线：
 *   1. 十六分法收集可见方块
 *   2. 4D→3D：xzw 立方体与观察平面求交 → 三棱柱面片
 *   3. 3D→2D：标准透视投影 + z-buffer 光栅化
 */
class Renderer
{
public:
    Renderer(int screenWidth, int screenHeight, double scale = 400.0);
    ~Renderer();

    /** @brief 主渲染入口 */
    void renderWorld(const World &world, const Camera4D &cam);

    /** @brief 绘制准星 */
    void drawCrosshair() const;

    /** @brief 绘制 HUD 信息 */
    void drawHUD(const Camera4D &cam) const;

    /** @brief 从 assert/texture/grass_block/ 加载纹理颜色 */
    void loadTextures(const wchar_t *basePath);

    /** @brief 添加超方块（不展开，渲染时 16 分法遍历） */
    void addSuperBlock(const SuperBlock &sb)
    {
        m_superBlocks.push_back(sb);
        m_sbGrid.insert(sb.pos());
    }

private:
    // ---- 屏幕参数 ----
    int m_screenWidth, m_screenHeight;
    double m_scale;
    double m_offsetX, m_offsetY;
    double m_blockHalf;
    int m_frameCount;

    // ---- DIB 离屏缓冲 ----
    HBITMAP m_hBmp;
    HDC m_memDC;
    HBITMAP m_oldBmp;
    DWORD *m_pBits;
    bool m_dibReady;

    // ---- z-buffer ----
    std::vector<double> m_zbuf;

    // ---- FPS ----
    int m_fpsFrames;
    clock_t m_fpsTime;
    int m_fps;

    // ---- 诊断 ----
    int m_diagBlocks;
    int m_diagVisible;
    int m_diagTriangles;

    // ---- 纹理 ----
    COLORREF m_tex[16][16][16][16];
    bool m_texLoaded;

    // ---- 超方块 ----
    std::vector<SuperBlock> m_superBlocks;
    std::unordered_set<IVec4> m_sbGrid;

    // ---- 内部方法 ----

    /** @brief 清空帧缓冲 */
    void resetBuffers();

    /** @brief 获取方块颜色 */
    COLORREF getBlockColor(int x, int y, int z, int w) const;

    /**
     * @brief 收集所有需要渲染的可见方块
     * @return (bx, by, bz, bw) 列表
     */
    std::vector<IVec4> collectVisibleBlocks(const World &world, const Camera4D &cam,
        const Plane2D &plane);

    /**
     * @brief 超方块十六分法递归遍历
     */
    void traverseSuperBlock(const SuperBlock &sb, const Camera4D &cam,
        const Plane2D &plane, const World &world,
        std::vector<IVec4> &outBlocks);

    /**
     * @brief 4D→3D：单方块 → 三角形列表
     */
    void blockToTriangles(int bx, int by, int bz, int bw,
        const Camera4D &cam, const Plane2D &plane,
        COLORREF color, std::vector<Tri3D> &outTris);

    /**
     * @brief 3D→2D：光栅化所有三角形
     */
    void rasterizeTriangles(const std::vector<Tri3D> &tris,
        const Camera3D &cam3d);

    /** @brief 光栅化单个三角形 */
    void rasterizeTriangle(const Tri3D &tri, const Camera3D &cam3d);

    /** @brief 扫描线填充 */
    void drawScanline(int y, int x0, int x1, double z0, double z1, COLORREF color);
};
