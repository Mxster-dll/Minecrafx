#pragma once

#include "../core/constant.h"
#include <vector>
#include <unordered_map>

struct CraftSlot
{
    int blockType;
};

struct ShapelessRecipe
{
    std::vector<int> inputs;
    int outputType;
    int outputCount;
};

struct ShapedRecipe
{
    int pattern[3][3];
    int recipeW;
    int recipeH;
    int outputType;
    int outputCount;
};

struct CraftResult
{
    bool valid = false;
    int outputType = BLOCK_AIR;
    int outputCount = 0;
    bool isShaped = false;
};

class CraftingManager
{
public:
    CraftingManager();

    void addShapeless(const std::vector<int> &inputs, int outputType, int outputCount);

    void addShaped(const int pattern[3][3], int w, int h, int outputType, int outputCount);

    CraftResult match(const int grid[3][3]) const;

private:
    std::vector<ShapelessRecipe> m_shapeless;
    std::vector<ShapedRecipe>   m_shaped;
};

std::unordered_map<int, int> countItems3x3(const int grid[3][3]);

std::unordered_map<int, int> countItemsVec(const std::vector<int> &items);