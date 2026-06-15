#pragma once

constexpr int    SCREEN_WIDTH = 800;
constexpr int    SCREEN_HEIGHT = 600;
constexpr double MOVE_SPEED = 5.0;           // 移动速度（单位/秒）
constexpr double MOUSE_SENSITIVITY = 0.003;  // 鼠标视角灵敏度（弧度/像素）
constexpr double SLICE_STEP = 0.03;          // 切片旋转每格步长（弧度），滚轮/Q/E 统一
constexpr double SLICE_SMOOTH = 0.28;        // 速度平滑系数（0~1，越大越快）

// 碰撞常量（方块半边长 0.5，玩家碰撞球半径）
inline constexpr double PLAYER_R = 0.25;