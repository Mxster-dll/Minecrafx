#pragma once

#include "world.h"
#include "camera.h"
#include "linalg.h"
#include <graphics.h>

/**
 * @brief 4D 线框渲染器
 *
 * 按 3D 切片方式渲染：先过滤与切片相交的方块，再将其渲染为 3D 立方体。
 */
class Renderer
{
public:
    Renderer(int screenWidth, int screenHeight, double scale = 400.0);

    /** @brief 渲染世界（切片过滤 + 3D 立方体） */
    void renderWorld(const World &world, const Camera4D &cam);

    /** @brief 绘制屏幕中央十字准星 */
    void drawCrosshair() const;

    /** @brief 绘制 HUD 信息 */
    void drawHUD(const Camera4D &cam) const;

private:
    /**
     * @brief 用 3D 立方体渲染一个方块（仅 X/Y/Z 变化，W 固定）
     * @param blockPos  方块整数坐标
     * @param cam       摄像机
     * @param overDist  方块中心在 over 方向到切片的距离
     */
    void drawBlock3D(const IVec4 &blockPos, const Camera4D &cam, double overDist);

    int m_screenWidth;
    int m_screenHeight;
    double m_scale;
    double m_offsetX;
    double m_offsetY;

    /** 3D 立方体 12 条边 */
    static const int CUBE_EDGES[12][2];
};
