#pragma once

#include "constant.h"
#include <unordered_map>

// ============================================================================
// FurnaceManager — 熔炉烧炼系统
// ============================================================================
class FurnaceManager
{
public:
    FurnaceManager();

    // ---- 配方查询 ----
    /** @brief 返回原料的烧炼产物，不可烧炼则返回 BLOCK_AIR */
    static int smeltResult(int inputType);
    /** @brief 返回燃料可烧制的物品数 */
    static double fuelValue(int itemType);

    // ---- 熔炉状态（每台熔炉一个实例，或嵌入 Inventory） ----
    struct State
    {
        int inputType = BLOCK_AIR;   // 输入格物品
        int inputCount = 0;
        int fuelType = BLOCK_AIR;    // 燃料格物品
        int fuelCount = 0;
        int outputType = BLOCK_AIR;  // 输出格产物
        int outputCount = 0;
        double burnProgress = 0.0;   // 当前物品烧制进度 0~1
        double burnTimeRemain = 0.0; // 剩余燃料时间（可烧制物品数）
        double fuelCapacity = 0.0;   // 当前燃料总容量（用于进度条比例）
        bool active = false;         // 是否正在燃烧
    };

    // ---- 模拟一帧烧制 ----
    /** @brief 更新熔炉状态，dt 为帧间隔（秒），返回 true 表示仍在燃烧 */
    static bool update(State &st, double dt);
};

// ============================================================================
// 烧炼配方表
// ============================================================================
inline int FurnaceManager::smeltResult(int inputType)
{
    static const std::unordered_map<int, int> map = {
        { BLOCK_DIAMOND_ORE,      BLOCK_DIAMOND },
        { BLOCK_GOLD_ORE,         BLOCK_GOLD_INGOT },
        { BLOCK_IRON_ORE,         BLOCK_IRON_INGOT },
        { BLOCK_COAL_ORE,         BLOCK_COAL },
        { BLOCK_COBBLESTONE,      BLOCK_STONE },
        { BLOCK_LOG,              BLOCK_COAL },     // 原木 → 煤炭
        // 铁工具/盔甲 → 铁粒
        { BLOCK_IRON_PICKAXE,     BLOCK_IRON_NUGGET },
        { BLOCK_IRON_AXE,         BLOCK_IRON_NUGGET },
        { BLOCK_IRON_SHOVEL,      BLOCK_IRON_NUGGET },
        { BLOCK_IRON_SWORD,       BLOCK_IRON_NUGGET },
        { BLOCK_IRON_HOE,         BLOCK_IRON_NUGGET },
        { BLOCK_IRON_HELMET,      BLOCK_IRON_NUGGET },
        { BLOCK_IRON_CHESTPLATE,  BLOCK_IRON_NUGGET },
        { BLOCK_IRON_LEGGINGS,    BLOCK_IRON_NUGGET },
        { BLOCK_IRON_BOOTS,       BLOCK_IRON_NUGGET },
        // 金工具/盔甲 → 金粒
        { BLOCK_GOLDEN_PICKAXE,   BLOCK_GOLD_NUGGET },
        { BLOCK_GOLDEN_AXE,       BLOCK_GOLD_NUGGET },
        { BLOCK_GOLDEN_SHOVEL,    BLOCK_GOLD_NUGGET },
        { BLOCK_GOLDEN_SWORD,     BLOCK_GOLD_NUGGET },
        { BLOCK_GOLDEN_HOE,       BLOCK_GOLD_NUGGET },
        { BLOCK_GOLDEN_HELMET,    BLOCK_GOLD_NUGGET },
        { BLOCK_GOLDEN_CHESTPLATE,BLOCK_GOLD_NUGGET },
        { BLOCK_GOLDEN_LEGGINGS,  BLOCK_GOLD_NUGGET },
        { BLOCK_GOLDEN_BOOTS,     BLOCK_GOLD_NUGGET },
    };
    auto it = map.find(inputType);
    return (it != map.end()) ? it->second : BLOCK_AIR;
}

// ============================================================================
// 燃料热值表
// ============================================================================
inline double FurnaceManager::fuelValue(int itemType)
{
    static const std::unordered_map<int, double> map = {
        { BLOCK_LOG,              1.5 },
        { BLOCK_PLANKS,           1.5 },
        { BLOCK_STICK,            0.5 },
        { BLOCK_WOODEN_PICKAXE,   1.0 },
        { BLOCK_WOODEN_AXE,       1.0 },
        { BLOCK_WOODEN_SHOVEL,    1.0 },
        { BLOCK_WOODEN_SWORD,     1.0 },
        { BLOCK_WOODEN_HOE,       1.0 },
        { BLOCK_COAL,             8.0 },
        { BLOCK_COAL_BLOCK,       80.0 },
    };
    auto it = map.find(itemType);
    return (it != map.end()) ? it->second : 0.0;
}
