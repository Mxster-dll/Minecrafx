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

    // 耗时累加器 — 每项对应一步原子操作
    mutable clock_t m_timeZBuf;    // 清空深度缓冲
    mutable clock_t m_timeDIB;     // 创建DIB+填充背景
    mutable clock_t m_timeCellTest;// 胞腔-切片相交测试(16分法)
    mutable clock_t m_timeSurfChk; // 表面方块判断
    mutable clock_t m_timeVertGen; // 生成16个顶点
    mutable clock_t m_timeOverDot; // 16次 over·vertex 点积
    mutable clock_t m_time24Face;  // 24面边跨越检测+求交点
    mutable clock_t m_timeCellGrp; // 胞腔分组(交线段归入8胞腔)
    mutable clock_t m_timeEpiMatch;// 端点匹配(next[]链)
    mutable clock_t m_timeChain;   // 链追踪+投影
    mutable clock_t m_timeDSort;   // 面深度汇总
    mutable clock_t m_timeSort;    // 深度排序
    mutable clock_t m_timeBBOX;    // 多边形包围盒
    mutable clock_t m_timeEdges;   // 扫描线边求交
    mutable clock_t m_timePixWr;   // 逐像素z-buffer写入
    mutable clock_t m_timeBitBlt;  // BitBlt刷屏
    mutable clock_t m_timeWorld;
    mutable clock_t m_timeElapsed;
    mutable clock_t m_tPrev;
    int m_timeSamples;

    COLORREF m_tex[16][16][16][16];
    bool m_texLoaded;

    std::vector<SuperBlock> m_superBlocks;

    void resetBuffers();
    void fillPolygonZ(const POINT *pts, int n, const double *depths, COLORREF color);
    static const int FACES[24][4];
};
