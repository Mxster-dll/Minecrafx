#pragma once

constexpr int    SCREEN_WIDTH = 800;
constexpr int    SCREEN_HEIGHT = 600;
constexpr double MOVE_SPEED = 5.0;           // 移动速度（单位/秒）
constexpr double MOUSE_SENSITIVITY = 0.003;  // 鼠标视角灵敏度（弧度/像素）
constexpr double SLICE_STEP = 0.03;          // 切片旋转每格步长（弧度），滚轮/Q/E 统一
constexpr double SLICE_SMOOTH = 0.28;        // 速度平滑系数（0~1，越大越快）

// 方块类型
enum BlockType : int
{
    BLOCK_AIR = 0,
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_LOG = 3,
    BLOCK_LEAVES = 4,
};

// 碰撞常量
inline constexpr double CYLINDER_R = 0.4;    // 玩家圆柱半径（xzw 空间）
inline constexpr double CYLINDER_H = 1.6;    // 玩家圆柱高度（Y 方向，摄像机在顶部）