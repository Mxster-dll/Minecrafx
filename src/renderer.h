#pragma once

#include "world.h"
#include "camera.h"
#include "linalg.h"
#include <graphics.h>

/**
 * @brief 4D 线框渲染器
 *
 * 负责将 World 中的方块投影到屏幕，按距离排序后绘制超立方体线框。
 */
class Renderer
{
public:
    /**
     * @brief 构造渲染器
     * @param screenWidth  屏幕宽度
     * @param screenHeight 屏幕高度
     * @param scale        投影缩放因子
     */
    Renderer(int screenWidth, int screenHeight, double scale = 400.0);

    /**
     * @brief 渲染整个世界
     * @param world 世界数据
     * @param cam   摄像机
     */
    void renderWorld(const World &world, const Camera4D &cam);

    /**
     * @brief 绘制屏幕中央十字准星
     */
    void drawCrosshair() const;

    /**
     * @brief 绘制 HUD 信息
     * @param cam 摄像机（读取位置和基向量信息）
     */
    void drawHUD(const Camera4D &cam) const;

private:
    /**
     * @brief 绘制单个方块的超立方体线框
     * @param blockPos 方块整数坐标
     * @param cam      摄像机
     */
    void drawBlock(const IVec4 &blockPos, const Camera4D &cam);

    /**
     * @brief 生成超立方体的 16 个世界空间顶点
     * @param centerX 方块中心 X 坐标（整数）
     * @param centerY 方块中心 Y 坐标（整数）
     * @param centerZ 方块中心 Z 坐标（整数）
     * @param centerW 方块中心 W 坐标（整数）
     * @param vertices 输出：16 个 Vec4 顶点
     */
    static void generateHypercubeVertices(int centerX, int centerY, int centerZ, int centerW,
        Vec4 vertices[16]);

    int m_screenWidth;
    int m_screenHeight;
    double m_scale;
    double m_offsetX;
    double m_offsetY;

    /** 超立方体 32 条边的端点索引（静态查找表） */
    static const int EDGE_TABLE[32][2];
};
