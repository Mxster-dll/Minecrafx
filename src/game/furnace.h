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
        { BLOCK_DIAMOND_ORE,      ITEM_DIAMOND },
        { BLOCK_GOLD_ORE,         ITEM_GOLD_INGOT },
        { BLOCK_IRON_ORE,         ITEM_IRON_INGOT },
        { BLOCK_COAL_ORE,         ITEM_COAL },
        { BLOCK_COBBLESTONE,      BLOCK_STONE },
        { BLOCK_LOG,              ITEM_COAL },

        { ITEM_IRON_PICKAXE,     ITEM_IRON_NUGGET },
        { ITEM_IRON_AXE,         ITEM_IRON_NUGGET },
        { ITEM_IRON_SHOVEL,      ITEM_IRON_NUGGET },
        { ITEM_IRON_SWORD,       ITEM_IRON_NUGGET },
        { ITEM_IRON_HOE,         ITEM_IRON_NUGGET },
        { ITEM_IRON_HELMET,      ITEM_IRON_NUGGET },
        { ITEM_IRON_CHESTPLATE,  ITEM_IRON_NUGGET },
        { ITEM_IRON_LEGGINGS,    ITEM_IRON_NUGGET },
        { ITEM_IRON_BOOTS,       ITEM_IRON_NUGGET },

        { ITEM_GOLDEN_PICKAXE,   ITEM_GOLD_NUGGET },
        { ITEM_GOLDEN_AXE,       ITEM_GOLD_NUGGET },
        { ITEM_GOLDEN_SHOVEL,    ITEM_GOLD_NUGGET },
        { ITEM_GOLDEN_SWORD,     ITEM_GOLD_NUGGET },
        { ITEM_GOLDEN_HOE,       ITEM_GOLD_NUGGET },
        { ITEM_GOLDEN_HELMET,    ITEM_GOLD_NUGGET },
        { ITEM_GOLDEN_CHESTPLATE,ITEM_GOLD_NUGGET },
        { ITEM_GOLDEN_LEGGINGS,  ITEM_GOLD_NUGGET },
        { ITEM_GOLDEN_BOOTS,     ITEM_GOLD_NUGGET },
    };
    auto it = map.find(inputType);
    return (it != map.end()) ? it->second : BLOCK_AIR;
}

inline double FurnaceManager::fuelValue(int itemType)
{
    static const std::unordered_map<int, double> map = {
        { BLOCK_LOG,              1.5 },
        { BLOCK_PLANKS,           1.5 },
        { ITEM_STICK,            0.5 },
        { ITEM_WOODEN_PICKAXE,   1.0 },
        { ITEM_WOODEN_AXE,       1.0 },
        { ITEM_WOODEN_SHOVEL,    1.0 },
        { ITEM_WOODEN_SWORD,     1.0 },
        { ITEM_WOODEN_HOE,       1.0 },
        { ITEM_COAL,             8.0 },
        { BLOCK_COAL_BLOCK,       80.0 },
    };
    auto it = map.find(itemType);
    return (it != map.end()) ? it->second : 0.0;
}