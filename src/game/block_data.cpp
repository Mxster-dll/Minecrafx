#include "block_data.h"

static inline bool isPickaxe(int t)
{
    return t == ITEM_WOODEN_PICKAXE || t == ITEM_STONE_PICKAXE ||
        t == ITEM_IRON_PICKAXE || t == ITEM_DIAMOND_PICKAXE ||
        t == ITEM_GOLDEN_PICKAXE;
}

static inline bool isAxe(int t)
{
    return t == ITEM_WOODEN_AXE || t == ITEM_STONE_AXE ||
        t == ITEM_IRON_AXE || t == ITEM_DIAMOND_AXE ||
        t == ITEM_GOLDEN_AXE;
}

static inline bool isShovel(int t)
{
    return t == ITEM_WOODEN_SHOVEL || t == ITEM_STONE_SHOVEL ||
        t == ITEM_IRON_SHOVEL || t == ITEM_DIAMOND_SHOVEL ||
        t == ITEM_GOLDEN_SHOVEL;
}

static inline bool isSword(int t)
{
    return t == ITEM_WOODEN_SWORD || t == ITEM_STONE_SWORD ||
        t == ITEM_IRON_SWORD || t == ITEM_DIAMOND_SWORD ||
        t == ITEM_GOLDEN_SWORD;
}

static inline double pickSpeed(int t, double base)
{
    if (t == ITEM_GOLDEN_PICKAXE)  return base * 0.15;
    if (t == ITEM_DIAMOND_PICKAXE) return base * 0.3;
    if (t == ITEM_IRON_PICKAXE)    return base * 0.4;
    if (t == ITEM_STONE_PICKAXE)   return base * 0.5;
    return base;
}

static inline double axeSpeed(int t, double base)
{
    if (t == ITEM_GOLDEN_AXE)  return base * 0.2;
    if (t == ITEM_DIAMOND_AXE) return base * 0.25;
    if (t == ITEM_IRON_AXE)    return base * 0.35;
    if (t == ITEM_STONE_AXE)   return base * 0.55;
    return base;
}

static inline double shovelSpeed(int t, double base)
{
    if (t == ITEM_GOLDEN_SHOVEL)  return base * 0.12;
    if (t == ITEM_DIAMOND_SHOVEL) return base * 0.15;
    if (t == ITEM_IRON_SHOVEL)    return base * 0.25;
    if (t == ITEM_STONE_SHOVEL)   return base * 0.4;
    return base * 0.6;
}

static inline int pickTier(int t)
{
    if (t == ITEM_WOODEN_PICKAXE || t == ITEM_GOLDEN_PICKAXE) return 0;
    if (t == ITEM_STONE_PICKAXE)   return 1;
    if (t == ITEM_IRON_PICKAXE)    return 2;
    if (t == ITEM_DIAMOND_PICKAXE) return 3;
    return -1;
}

double getMiningTime(int blockType, int toolType)
{
    switch (blockType)
    {
        case BLOCK_GRASS:
            if (isShovel(toolType)) return shovelSpeed(toolType, 0.5);
            return 0.9;
        case BLOCK_DIRT:
            if (isShovel(toolType)) return shovelSpeed(toolType, 0.4);
            return 0.8;
        case BLOCK_LOG:
        case BLOCK_PLANKS:
            if (isAxe(toolType)) return axeSpeed(toolType, 1.5);
            return 3.0;
        case BLOCK_LEAVES:
            if (isSword(toolType)) return 0.1;
            return 0.3;
        case BLOCK_STONE:
            if (isPickaxe(toolType)) return pickSpeed(toolType, 1.2);
            return 7.5;
        case BLOCK_CRAFTING_TABLE:
            if (isAxe(toolType)) return axeSpeed(toolType, 1.9);
            return 3.8;
        case BLOCK_DIAMOND_ORE:
        case BLOCK_GOLD_ORE:
            if (isPickaxe(toolType)) return pickSpeed(toolType, 1.5);
            return 15.0;
        case BLOCK_IRON_ORE:
            if (isPickaxe(toolType)) return pickSpeed(toolType, 0.8);
            return 15.0;
        case BLOCK_DIAMOND_BLOCK:
        case BLOCK_IRON_BLOCK:
            if (isPickaxe(toolType)) return pickSpeed(toolType, 1.3);
            return 25.0;
        case BLOCK_GOLD_BLOCK:
            if (isPickaxe(toolType)) return pickSpeed(toolType, 0.8);
            return 15.0;
        case BLOCK_COBBLESTONE:
            if (isPickaxe(toolType)) return pickSpeed(toolType, 1.2);
            return 7.5;
        case BLOCK_COAL_ORE:
            if (isPickaxe(toolType)) return pickSpeed(toolType, 0.8);
            return 15.0;
        case BLOCK_COAL_BLOCK:
            if (isPickaxe(toolType)) return pickSpeed(toolType, 1.3);
            return 15.0;
        case BLOCK_FURNACE:
            if (isPickaxe(toolType)) return pickSpeed(toolType, 1.5);
            return 17.5;
        default:
            return 1.0;
    }
}

bool canHarvest(int blockType, int toolType)
{
    switch (blockType)
    {
        case BLOCK_STONE:
        case BLOCK_COBBLESTONE:
        case BLOCK_COAL_ORE:
        case BLOCK_COAL_BLOCK:
        case BLOCK_FURNACE:
        case BLOCK_DIAMOND_ORE:
        case BLOCK_GOLD_ORE:
        case BLOCK_IRON_ORE:
        case BLOCK_DIAMOND_BLOCK:
        case BLOCK_GOLD_BLOCK:
        case BLOCK_IRON_BLOCK:
            if (!isPickaxe(toolType)) return false;
            break;
        default: break;
    }
    int tier = pickTier(toolType);
    switch (blockType)
    {
        case BLOCK_IRON_ORE:    return tier >= 1;
        case BLOCK_GOLD_ORE:
        case BLOCK_DIAMOND_ORE:
        case BLOCK_DIAMOND_BLOCK:
        case BLOCK_GOLD_BLOCK:
        case BLOCK_IRON_BLOCK:  return tier >= 2;
        default:                return true;
    }
}