#pragma once

#include "world.h"
#include "camera.h"
#include "linalg.h"
#include "superblock.h"
#include <graphics.h>
#include <vector>
#include <ctime>

/**
 * @brief 4D 切片渲染器
 *
 * 用超平面截 4D 立方体，绘制真实的三维交线投影。
 */
class Renderer
{
public:
    Renderer(int screenWidth, int screenHeight, double scale = 400.0);
    ~Renderer();

    void renderWorld(const World &world, const Camera4D &cam);
    void drawCrosshair() const;
    void drawHUD(const Camera4D &cam) const;

    /** @brief 从 assert/texture/grass_block/ 加载纹理颜色 */
    void loadTextures(const wchar_t *basePath);

    /** @brief 添加超方块（不展开，渲染时 16 分法遍历） */
    void addSuperBlock(const SuperBlock &sb) { m_superBlocks.push_back(sb); }

private:
    void drawFacesStep(const World &world, const Camera4D &cam);

    COLORREF getBlockColor(int x, int y, int z, int w) const;

    int m_screenWidth, m_screenHeight;
    double m_scale, m_offsetX, m_offsetY;
    double m_blockHalf;
    int m_frameCount;
    std::vector<double> m_zbuf;
    DWORD *m_pBits;

    // FPS
    int m_fpsFrames;
    clock_t m_fpsTime;
    int m_fps;

    // 诊断计数器
    int m_diagTotal;
    int m_diagSlice;
    int m_diagOccl;
    int m_diagGeom;
    int m_diagFaces;

    // 耗时累加器（mutable 允许 const 函数中更新）
    mutable clock_t m_timeClear;
    mutable clock_t m_timeIter;
    mutable clock_t m_timeOccl;
    mutable clock_t m_timeGeom;
    mutable clock_t m_timeRast;
    mutable clock_t m_timeWorld;   // renderWorld 总耗时
    mutable clock_t m_timeElapsed; // 实际帧间耗时
    mutable clock_t m_tPrev;       // 上一帧 clock
    int m_timeSamples;

    COLORREF m_tex[16][16][16][16];
    bool m_texLoaded;

    std::vector<SuperBlock> m_superBlocks;

    void resetBuffers();
    void fillPolygonZ(const POINT *pts, int n, const double *depths, COLORREF color);
    static const int FACES[24][4];
};
