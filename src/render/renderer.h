#pragma once

#include "../core/constant.h"
#include "../world/world.h"
#include "../world/camera.h"
#include "../core/linalg.h"
#include "../world/project4d.h"
#include <graphics.h>
#include <vector>
#include <unordered_set>
#include <atomic>
#include <ctime>

class ThreadPool;

class Renderer
{
public:
    Renderer(int screenWidth, int screenHeight);
    ~Renderer();

    void renderWorld(const World &world, const Camera4D &cam);

    void drawCrosshair();

    void drawHUD(const Camera4D &cam);

    void toggleHUD() { m_showHUD = !m_showHUD; }

    void drawBlockOutline(const IVec4 &blockPos, const Camera4D &cam);

    void setTargetBlock(const IVec4 &pos) { m_targetBlock = pos; m_hasTarget = true; }
    void clearTargetBlock() { m_hasTarget = false; }

    void setMiningTarget(const IVec4 &block, double progress)
    {
        m_miningTarget = block;
        m_miningStage = (progress > 0.0 && progress < 1.0)
            ? (int) (progress * 10.0) : -1;
        if (m_miningStage > 9) m_miningStage = 9;
    }
    void clearMiningTarget() { m_miningStage = -1; }

    static void setFurnaceActive(bool active) { m_furnaceActive = active; }

    void loadBlockTextures();

    void loadDestroyStages();

    static int blockTexId(int blockType, int face);

    COLORREF sampleTexture(int texId, double tu, double tv) const;

    void loadHotbar();

    void drawHotbar(int selectedSlot, const int *hotbarBlockTypes = nullptr,
        const int *hotbarCounts = nullptr);

    void drawBlockIcon(int screenX, int screenY, int size, int blockType, int count = 1);

    void loadInventoryIcons();

    DWORD *getPixelBits() const { return m_pBits; }
    int getScreenWidth() const { return m_screenWidth; }
    int getScreenHeight() const { return m_screenHeight; }
    bool isDibReady() const { return m_dibReady; }

    void captureBackground();

    void applyGaussianBlur();

    void drawBackground();

    void flushToScreen();

    void drawImageCentered(IMAGE *img);

    void drawButton(int x, int y, int w, int h,
        IMAGE *imgNormal, IMAGE *imgHover, IMAGE *imgActive,
        const wchar_t *text, bool hovered, bool pressed);

private:

    int m_screenWidth, m_screenHeight;
    double m_blockHalf;
    int m_frameCount;

    class ThreadPool *m_pool;

    HBITMAP m_hBmp;
    HDC m_memDC;
    HBITMAP m_oldBmp;
    DWORD *m_pBits;
    bool m_dibReady;

    HFONT m_hFont;
    HFONT m_hFontLarge;
    HFONT m_hOldFont;

    std::vector<double> m_zbuf;

    int m_fpsFrames;
    clock_t m_fpsTime;
    int m_fps;

    int m_diagTotal;
    int m_diagSlice;
    int m_diagOccl;
    int m_diagGeom;
    int m_diagFaces;
    std::atomic<int> m_diagFaceCull;
    int m_diagChunkTotal;
    int m_diagChunkPass;
    int m_diagThreads;
    int m_diagTiles;
    double m_msCollect;
    double m_msFrustum;
    double m_msBlock2Tri;
    double m_msRaster;

    static constexpr int MAX_TEX = 55;
    COLORREF m_texPixels[MAX_TEX][16][16];
    int m_texW[MAX_TEX], m_texH[MAX_TEX];
    bool m_blockTexLoaded;

    static constexpr int DESTROY_STAGES = 10;
    COLORREF m_destroyPixels[DESTROY_STAGES][16][16];
    bool m_destroyLoaded = false;

    static constexpr int HOTBAR_SLOTS = 9;
    static constexpr int HB_ICON_SIZE = 16;
    static constexpr int HB_HEIGHT = 44;
    static constexpr int HB_SLOT_ORIGIN_X = 3;
    static constexpr int HB_SLOT_ORIGIN_Y = 3;
    static constexpr int HB_SLOT_STEP = 20;
    static constexpr int HB_SLOT_SIZE = 16;
    int m_hotbarBlockTypes[HOTBAR_SLOTS];
    std::vector<COLORREF> m_hotbarBg;
    int m_hbBgW = 0, m_hbBgH = 0;
    std::vector<COLORREF> m_hotbarIcons[MAX_BLOCK_TYPE];
    std::vector<COLORREF> m_hotbarIconsBig[MAX_BLOCK_TYPE];
    int m_hbIconDisplaySize = 0;
    std::vector<COLORREF> m_selectPixels;
    int m_selectW = 0, m_selectH = 0;

    static constexpr int INV_ICON_MAX = 64;
    std::vector<COLORREF> m_invIcons[MAX_BLOCK_TYPE];
    int m_invIconW[MAX_BLOCK_TYPE] = {};
    int m_invIconH[MAX_BLOCK_TYPE] = {};

    bool m_hotbarLoaded = false;

    std::vector<DWORD> m_background;
    bool m_backgroundReady = false;

    bool m_showHUD = false;

    IVec4 m_targetBlock;
    bool m_hasTarget = false;

    IVec4 m_miningTarget;
    int m_miningStage = -1;

    static inline bool m_furnaceActive = false;

    void resetBuffers();

    std::vector<IVec4> collectVisibleBlocks(const World &world, const Camera4D &cam,
        const Plane2D &plane, int &outPreOccl);

    void blockToTriangles(int bx, int by, int bz, int bw,
        const Camera4D &cam, const Plane2D &plane,
        int topTexId, int sideTexId, int bottomTexId,
        std::vector<Tri3D> &outTris,
        const World &world, int destroyStage = -1);

    void rasterizeTriangles(const std::vector<Tri3D> &tris,
        const Camera3D &cam3d, int tileYMin = 0, int tileYMax = 99999);

    void rasterizeTriangle(const Tri3D &tri, const Camera3D &cam3d,
        int tileYMin = 0, int tileYMax = 99999);

    void drawScanline(int y, int x0, int x1, double z0, double z1,
        double tuz0, double tvz0, double ooz0,
        double tuz1, double tvz1, double ooz1,
        int texId, COLORREF color, int destroyStage = -1,
        int tileYMin = 0, int tileYMax = 99999);

    void drawOutlineEdge3D(double u0, double v0, double y0,
        double u1, double v1, double y1,
        const Camera3D &cam3d);
};