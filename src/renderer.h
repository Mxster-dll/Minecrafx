#pragma once

#include "world.h"
#include "camera.h"
#include "linalg.h"
#include <graphics.h>

/**
 * @brief 4D 切片渲染器
 *
 * 用超平面截 4D 立方体，绘制真实的三维交线投影。
 */
class Renderer
{
public:
    Renderer(int screenWidth, int screenHeight, double scale = 400.0);

    void renderWorld(const World& world, const Camera4D& cam);
    void drawCrosshair() const;
    void drawHUD(const Camera4D& cam) const;

private:
    /**
     * @brief 计算超平面与 4D 立方体的真实交线并绘制
     * @param bx,by,bz,bw  方块整数中心
     * @param cam          摄像机
     */
    void drawBlockSlice(int bx, int by, int bz, int bw, const Camera4D& cam);

    int m_screenWidth, m_screenHeight;
    double m_scale, m_offsetX, m_offsetY;

    /** 超立方体 24 个二维面的顶点索引 */
    static const int FACES[24][4];
};
