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
    Renderer(int screenWidth, int screenHeight);
    ~Renderer();

    /** @brief 主渲染入口 */
    void renderWorld(const World &world, const Camera4D &cam);

    /** @brief 绘制准星 */
    void drawCrosshair();

    /** @brief 绘制 HUD 信息 */
    void drawHUD(const Camera4D &cam);

    /** @brief 从 ../assert/texture/ 加载方块贴图（含像素数据） */
    void loadBlockTextures();

    /** @brief 由方块类型计算纹理 ID（face: 0=顶,1=侧,2=底） */
    static int blockTexId(int blockType, int face);

    /** @brief 采样纹理像素（texId 0~11） */
    COLORREF sampleTexture(int texId, double tu, double tv) const;

    /** @brief 加载热键栏素材 */
    void loadHotbar();

    /** @brief 绘制热键栏 */
    void drawHotbar(int selectedSlot);

    /** @brief 获取热键栏槽位对应的方块类型 */
    int getHotbarBlockType(int slot) const;

    /** @brief 添加超方块（不展开，渲染时 16 分法遍历） */
    void addSuperBlock(const SuperBlock &sb)
    {
        m_superBlocks.push_back(sb);
        m_sbGrid.insert(sb.pos());
    }

private:
    // ---- 屏幕参数 ----
    int m_screenWidth, m_screenHeight;
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
    int m_diagTotal;
    int m_diagSlice;
    int m_diagOccl;
    int m_diagGeom;
    int m_diagFaces;
    double m_msCollect;
    double m_msFrustum;
    double m_msBlock2Tri;
    double m_msRaster;

    // ---- 方块贴图 ----
    static constexpr int MAX_TEX = 18;
    COLORREF m_texPixels[MAX_TEX][16][16];  // 像素数据（最多 16×16）
    int m_texW[MAX_TEX], m_texH[MAX_TEX];
    bool m_blockTexLoaded;

    // ---- 热键栏 ----
    static constexpr int HOTBAR_SLOTS = 6;
    static constexpr int HB_ICON_SIZE = 32;      // 图标显示大小
    static constexpr int HB_HEIGHT = 44;          // 热键栏显示高度
    int m_hotbarBlockTypes[HOTBAR_SLOTS];
    std::vector<COLORREF> m_hotbarBg;             // 热键栏背景像素（堆分配）
    int m_hbBgW = 0, m_hbBgH = 0;                  // 背景原始宽高
    std::vector<COLORREF> m_hotbarIcons[HOTBAR_SLOTS]; // 32x32 快捷栏图标
    std::vector<COLORREF> m_hotbarIconsBig[HOTBAR_SLOTS]; // 64x64 右下角大图标
    bool m_hotbarLoaded = false;

    // ---- 超方块 ----
    std::vector<SuperBlock> m_superBlocks;
    std::unordered_set<IVec4> m_sbGrid;

    // ---- 内部方法 ----

    /** @brief 清空帧缓冲 */
    void resetBuffers();

    /**
     * @brief 收集所有需要渲染的可见方块
     * @return (bx, by, bz, bw) 列表
     */
    std::vector<IVec4> collectVisibleBlocks(const World &world, const Camera4D &cam,
        const Plane2D &plane, int &outPreOccl);

    /**
     * @brief 4D→3D：单方块 → 三角形列表（带纹理坐标）
     */
    void blockToTriangles(int bx, int by, int bz, int bw,
        const Camera4D &cam, const Plane2D &plane,
        int topTexId, int sideTexId, int bottomTexId,
        std::vector<Tri3D> &outTris);

    /**
     * @brief 3D→2D：光栅化所有三角形
     */
    void rasterizeTriangles(const std::vector<Tri3D> &tris,
        const Camera3D &cam3d);

    /** @brief 光栅化单个三角形 */
    void rasterizeTriangle(const Tri3D &tri, const Camera3D &cam3d);

    /** @brief 扫描线填充（透视校正纹理） */
    void drawScanline(int y, int x0, int x1, double z0, double z1,
        double tuz0, double tvz0, double ooz0,
        double tuz1, double tvz1, double ooz1,
        int texId, COLORREF color);
};
