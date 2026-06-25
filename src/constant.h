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

    // ── 自然方块 ──
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_LOG = 3,
    BLOCK_LEAVES = 4,
    BLOCK_STONE = 5,
    BLOCK_PLANKS = 6,

    // ── 基础物品 ──
    BLOCK_STICK = 7,
    BLOCK_CRAFTING_TABLE = 8,

    // ── 矿石 ──
    BLOCK_DIAMOND_ORE = 9,
    BLOCK_GOLD_ORE = 10,
    BLOCK_IRON_ORE = 11,

    // ── 矿物块 ──
    BLOCK_DIAMOND_BLOCK = 12,
    BLOCK_GOLD_BLOCK = 13,
    BLOCK_IRON_BLOCK = 14,

    // ── 矿物材料 ──
    BLOCK_DIAMOND = 15,
    BLOCK_GOLD_INGOT = 16,
    BLOCK_IRON_INGOT = 17,
    BLOCK_GOLD_NUGGET = 18,
    BLOCK_IRON_NUGGET = 19,

    // ── 食物 ──
    BLOCK_APPLE = 20,
    BLOCK_GOLDEN_APPLE = 21,

    // ── 木工具 ──
    BLOCK_WOODEN_PICKAXE = 22,
    BLOCK_WOODEN_AXE = 23,
    BLOCK_WOODEN_SHOVEL = 24,
    BLOCK_WOODEN_SWORD = 25,
    BLOCK_WOODEN_HOE = 26,

    // ── 石工具 ──
    BLOCK_STONE_PICKAXE = 27,
    BLOCK_STONE_AXE = 28,
    BLOCK_STONE_SHOVEL = 29,
    BLOCK_STONE_SWORD = 30,
    BLOCK_STONE_HOE = 31,

    // ── 铁工具 ──
    BLOCK_IRON_PICKAXE = 32,
    BLOCK_IRON_AXE = 33,
    BLOCK_IRON_SHOVEL = 34,
    BLOCK_IRON_SWORD = 35,
    BLOCK_IRON_HOE = 36,

    // ── 铁护甲 ──
    BLOCK_IRON_HELMET = 37,
    BLOCK_IRON_CHESTPLATE = 38,
    BLOCK_IRON_LEGGINGS = 39,
    BLOCK_IRON_BOOTS = 40,

    // ── 金工具 ──
    BLOCK_GOLDEN_PICKAXE = 41,
    BLOCK_GOLDEN_AXE = 42,
    BLOCK_GOLDEN_SHOVEL = 43,
    BLOCK_GOLDEN_SWORD = 44,
    BLOCK_GOLDEN_HOE = 45,

    // ── 金护甲 ──
    BLOCK_GOLDEN_HELMET = 46,
    BLOCK_GOLDEN_CHESTPLATE = 47,
    BLOCK_GOLDEN_LEGGINGS = 48,
    BLOCK_GOLDEN_BOOTS = 49,

    // ── 钻石工具 ──
    BLOCK_DIAMOND_PICKAXE = 50,
    BLOCK_DIAMOND_AXE = 51,
    BLOCK_DIAMOND_SHOVEL = 52,
    BLOCK_DIAMOND_SWORD = 53,
    BLOCK_DIAMOND_HOE = 54,

    // ── 钻石护甲 ──
    BLOCK_DIAMOND_HELMET = 55,
    BLOCK_DIAMOND_CHESTPLATE = 56,
    BLOCK_DIAMOND_LEGGINGS = 57,
    BLOCK_DIAMOND_BOOTS = 58,

    MAX_BLOCK_TYPE = 59  // 方块类型总数（数组大小用）
};

// 碰撞常量
inline constexpr double CYLINDER_R = 0.4;    // 玩家圆柱半径（xzw 空间）
inline constexpr double CYLINDER_H = 1.6;    // 玩家圆柱高度（Y 方向，摄像机在顶部）