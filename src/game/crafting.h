#pragma once

#include "../core/constant.h"
#include <vector>
#include <unordered_map>

// ============================================================================
// 配方原料（有序配方中的一个格子）
// ============================================================================
struct CraftSlot
{
    int blockType;  // BLOCK_AIR = 该格子必须为空
};

// ============================================================================
// 无形状配方：只看物品种类和数量，不关心位置
// ============================================================================
struct ShapelessRecipe
{
    std::vector<int> inputs;   // 所需方块的类型列表（可重复）
    int outputType;            // 输出方块类型
    int outputCount;           // 输出数量
};

// ============================================================================
// 有序配方：物品必须放在合成台格子的正确位置（任意子区域匹配）
// ============================================================================
struct ShapedRecipe
{
    int pattern[3][3];  // 配方图案（靠左上角存储，最大 3×3）
    int recipeW;        // 配方实际宽度（1~3）
    int recipeH;        // 配方实际高度（1~3）
    int outputType;
    int outputCount;
};

// ============================================================================
// 单次合成匹配结果
// ============================================================================
struct CraftResult
{
    bool valid = false;
    int outputType = BLOCK_AIR;
    int outputCount = 0;
    bool isShaped = false;   // true=有序配方, false=无形状配方
};

// ============================================================================
// 合成管理器：存储配方 + 匹配
// ============================================================================
class CraftingManager
{
public:
    CraftingManager();

    /** @brief 添加无形状配方（只关心数量和种类） */
    void addShapeless(const std::vector<int> &inputs, int outputType, int outputCount);

    /** @brief 添加有序配方（可任意尺寸 1×1 ~ 3×3，在合成区内滑动匹配） */
    void addShaped(const int pattern[3][3], int w, int h, int outputType, int outputCount);

    /**
     * @brief 尝试匹配合成表
     * @param grid  3×3 的方块类型数组（BLOCK_AIR 表示空格）
     * @return 匹配结果，未匹配时 valid=false
     */
    CraftResult match(const int grid[3][3]) const;

private:
    std::vector<ShapelessRecipe> m_shapeless;
    std::vector<ShapedRecipe>   m_shaped;
};

/** @brief 统计 3×3 网格中各类型的数量 */
std::unordered_map<int, int> countItems3x3(const int grid[3][3]);

/** @brief 统计输入列表中各类型的数量 */
std::unordered_map<int, int> countItemsVec(const std::vector<int> &items);
