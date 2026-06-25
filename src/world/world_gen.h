#pragma once

/**
 * @brief 计算 (x, z, w) 处的地形高度（用于生存模式丘陵生成）
 * @return 地表 y 坐标（整数）
 */
int terrainHeight(int x, int z, int w);

/**
 * @brief 生成生存模式世界：丘陵地形 + 树木 + 矿物
 * @param world   目标世界
 * @param MX,MZ,MW  世界范围 [0, MX) × [0, MZ) × [0, MW)
 */
void generateSurvivalWorld(class World &world, int MX, int MZ, int MW);

/**
 * @brief 生成创造模式世界：超平坦草地
 * @param world   目标世界
 * @param CX,CZ,CW  世界范围
 */
void generateCreativeWorld(class World &world, int CX, int CZ, int CW);

/**
 * @brief 为生存模式填充初始背包（首格铁镐）
 */
void initSurvivalInventory(class Inventory &inv);

/**
 * @brief 为创造模式填充背包（所有非工具/盔甲方块各 1 个）
 */
void initCreativeInventory(class Inventory &inv);
