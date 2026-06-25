#pragma once

#include "../core/constant.h"
#include <unordered_map>

class FurnaceManager
{
public:
    FurnaceManager();

    static int smeltResult(int inputType);

    static double fuelValue(int itemType);

    struct State
    {
        int inputType = BLOCK_AIR;
        int inputCount = 0;
        int fuelType = BLOCK_AIR;
        int fuelCount = 0;
        int outputType = BLOCK_AIR;
        int outputCount = 0;
        double burnProgress = 0.0;
        double burnTimeRemain = 0.0;
        double fuelCapacity = 0.0;
        bool active = false;
    };

    static bool update(State &st, double dt);
};

inline int FurnaceManager::smeltResult(int inputType)
{
    static const std::unordered_map<int, int> map = {
        { BLOCK_DIAMOND_ORE,      BLOCK_DIAMOND },
        { BLOCK_GOLD_ORE,         BLOCK_GOLD_INGOT },
        { BLOCK_IRON_ORE,         BLOCK_IRON_INGOT },
        { BLOCK_COAL_ORE,         BLOCK_COAL },
        { BLOCK_COBBLESTONE,      BLOCK_STONE },
        { BLOCK_LOG,              BLOCK_COAL },

        { BLOCK_IRON_PICKAXE,     BLOCK_IRON_NUGGET },
        { BLOCK_IRON_AXE,         BLOCK_IRON_NUGGET },
        { BLOCK_IRON_SHOVEL,      BLOCK_IRON_NUGGET },
        { BLOCK_IRON_SWORD,       BLOCK_IRON_NUGGET },
        { BLOCK_IRON_HOE,         BLOCK_IRON_NUGGET },
        { BLOCK_IRON_HELMET,      BLOCK_IRON_NUGGET },
        { BLOCK_IRON_CHESTPLATE,  BLOCK_IRON_NUGGET },
        { BLOCK_IRON_LEGGINGS,    BLOCK_IRON_NUGGET },
        { BLOCK_IRON_BOOTS,       BLOCK_IRON_NUGGET },

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