#pragma once

#include "world.h"
#include "camera.h"
#include "linalg.h"
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

    COLORREF m_tex[16][16][16][16];  // 纹理颜色表
    bool m_texLoaded;

    void resetBuffers();
    void fillPolygonZ(const POINT *pts, int n, const double *depths, COLORREF color);
    static const int FACES[24][4];
};
