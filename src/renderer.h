#pragma once

#include "world.h"
#include "camera.h"
#include "linalg.h"
#include <graphics.h>
#include <vector>

/**
 * @brief 4D 切片渲染器
 *
 * 用超平面截 4D 立方体，绘制真实的三维交线投影。
 */
class Renderer
{
public:
    Renderer(int screenWidth, int screenHeight, double scale = 400.0);

    void renderWorld(const World &world, const Camera4D &cam);
    void drawCrosshair() const;
    void drawHUD(const Camera4D &cam) const;

private:
    void drawBlockWire(int bx, int by, int bz, int bw,
        const Camera4D &cam, const World &world);
    void drawAllWires(const World &world, const Camera4D &cam);
    void drawFacesStep(const World &world, const Camera4D &cam);

    int m_screenWidth, m_screenHeight;
    double m_scale, m_offsetX, m_offsetY;
    int m_frameCount;
    std::vector<double> m_zbuf;
    DWORD *m_pBits;  // DIB 位图像素指针

    void resetBuffers();
    void fillPolygonZ(const POINT *pts, int n, const double *depths, COLORREF color);
    static const int FACES[24][4];
};
