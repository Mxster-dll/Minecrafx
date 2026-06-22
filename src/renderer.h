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

    /** @brief 切换 HUD 显示/隐藏 */
    void toggleHUD() { m_showHUD = !m_showHUD; }

    /** @brief 绘制目标方块的黑色线框（深度缓冲） */
    void drawBlockOutline(const IVec4 &blockPos, const Camera4D &cam);

    /** @brief 设置当前目标方块（每帧调用，传空 IVec4 清零） */
    void setTargetBlock(const IVec4 &pos) { m_targetBlock = pos; m_hasTarget = true; }
    void clearTargetBlock() { m_hasTarget = false; }

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

    /** @brief 获取原始像素缓冲区指针 */
    DWORD *getPixelBits() const { return m_pBits; }
    int getScreenWidth() const { return m_screenWidth; }
    int getScreenHeight() const { return m_screenHeight; }
    bool isDibReady() const { return m_dibReady; }

    /** @brief 从 DIB 捕获当前帧到背景缓冲区 */
    void captureBackground();

    /** @brief 对背景缓冲区应用高斯模糊 */
    void applyGaussianBlur();

    /** @brief 将模糊背景绘制到 DIB */
    void drawBackground();

    /** @brief 将 DIB 内容刷新到屏幕 */
    void flushToScreen();

    /** @brief 在屏幕正中央绘制一张图片（已加载的 IMAGE） */
    void drawImageCentered(IMAGE *img);

    /**
     * @brief 绘制按钮
     * @param x, y, w, h  按钮区域（屏幕坐标）
     * @param imgNormal, imgHover, imgActive  三种状态的贴图（可为 nullptr，则用纯色填充）
     * @param text        按钮文字
     * @param hovered     当前鼠标是否悬停
     * @param pressed     当前鼠标是否按下
     */
    void drawButton(int x, int y, int w, int h,
        IMAGE *imgNormal, IMAGE *imgHover, IMAGE *imgActive,
        const wchar_t *text, bool hovered, bool pressed);

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

    // ---- Minecraft AE 字体 ----
    HFONT m_hFont;       // HUD 小号字体（14pt）
    HFONT m_hFontLarge;  // 按钮大字字体（20pt）
    HFONT m_hOldFont;    // 原 DC 字体（用于析构恢复）

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
    int m_diagFaceCull;  // 面剔除跳过的面数
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
    static constexpr int HOTBAR_SLOTS = 9;
    static constexpr int HB_ICON_SIZE = 16;      // 图标显示大小
    static constexpr int HB_HEIGHT = 44;          // 热键栏显示高度
    // 每个槽位在 hotbar 原图中的位置：x = 3 + slot*20, y = 3, 大小 16×16
    static constexpr int HB_SLOT_ORIGIN_X = 3;
    static constexpr int HB_SLOT_ORIGIN_Y = 3;
    static constexpr int HB_SLOT_STEP = 20;
    static constexpr int HB_SLOT_SIZE = 16;
    int m_hotbarBlockTypes[HOTBAR_SLOTS];
    std::vector<COLORREF> m_hotbarBg;             // 热键栏背景像素（堆分配）
    int m_hbBgW = 0, m_hbBgH = 0;                  // 背景原始宽高
    std::vector<COLORREF> m_hotbarIcons[HOTBAR_SLOTS]; // 槽位图标像素（预缩放至显示尺寸）
    std::vector<COLORREF> m_hotbarIconsBig[HOTBAR_SLOTS]; // 右下角大图标
    int m_hbIconDisplaySize = 0;  // 槽位图标实际显示尺寸（像素）
    // 选中框
    std::vector<COLORREF> m_selectPixels;  // select.png 像素（预缩放）
    int m_selectW = 0, m_selectH = 0;      // select 显示尺寸
    bool m_hotbarLoaded = false;

    // ---- 超方块 ----
    std::vector<SuperBlock> m_superBlocks;
    std::unordered_set<IVec4> m_sbGrid;

    // ---- GUI 背景 ----
    std::vector<DWORD> m_background;
    bool m_backgroundReady = false;

    // ---- HUD 开关 ----
    bool m_showHUD = false;

    // ---- 目标方块 ----
    IVec4 m_targetBlock;
    bool m_hasTarget = false;

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
     * @brief 4D→3D：单方块 → 三角形列表（带纹理坐标，含面剔除）
     */
    void blockToTriangles(int bx, int by, int bz, int bw,
        const Camera4D &cam, const Plane2D &plane,
        int topTexId, int sideTexId, int bottomTexId,
        std::vector<Tri3D> &outTris,
        const World &world);

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

    /** @brief 绘制一条经深度测试的 3D 线段到 DIB */
    void drawOutlineEdge3D(double u0, double v0, double y0,
        double u1, double v1, double y1,
        const Camera3D &cam3d);
};
