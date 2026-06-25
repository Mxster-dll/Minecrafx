#pragma once

#include "../core/constant.h"

/**
 * @brief 获取挖掘指定方块所需的时间（秒）
 * @param blockType  方块类型（BLOCK_* 枚举）
 * @param toolType   手持工具类型（BLOCK_* 枚举）
 * @return 挖掘时间（秒），默认 1.0s
 *
 * 根据原版 Minecraft 数据缩放，考虑工具类型和材质加速系数：
 *   木=1.0, 石=0.5, 铁=0.4, 钻=0.3, 金=0.15
 */
double getMiningTime(int blockType, int toolType);

/**
 * @brief 检查当前工具能否正常采掘目标方块
 * @param blockType  方块类型
 * @param toolType   手持工具类型
 * @return true=可采掘（有掉落），false=不掉落
 *
 * 等级要求：
 *   石头/煤矿/熔炉等：必须用镐
 *   铁矿：石镐及以上（tier ≥ 1）
 *   金矿/钻石矿/金属块：铁镐及以上（tier ≥ 2）
 */
bool canHarvest(int blockType, int toolType);
