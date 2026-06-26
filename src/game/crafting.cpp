#include "crafting.h"
#include <cstdio>
#include <string>
#include <unordered_map>
#include <windows.h>

// TODO: 换库解析 json
static std::string readFile(const char *path)
{
    // HACK: 用 fopen 而非 ifstream —— MinGW 下 wchar_t 路径的 ifstream 有 bug
    FILE *f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string s(sz, '\0');
    fread(&s[0], 1, sz, f);
    fclose(f);
    return s;
}

static void skipWS(const std::string &s, size_t &pos)
{
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
        ++pos;
}

static std::string parseStr(const std::string &s, size_t &pos)
{
    skipWS(s, pos);
    if (pos >= s.size() || s[pos] != '"') return {};
    ++pos;
    size_t start = pos;
    while (pos < s.size() && s[pos] != '"') ++pos;
    std::string val = s.substr(start, pos - start);
    if (pos < s.size()) ++pos;
    return val;
}

static bool isNull(const std::string &s, size_t pos)
{
    return s.compare(pos, 4, "null") == 0;
}

static int parseBlockType(const std::string &name)
{
    // 这 map 越来越长，每次加方块都得来注册，烦
    // TODO: 用代码生成器自动生成
    static std::unordered_map<std::string, int> map = {
        {"air", BLOCK_AIR}, {"grass", BLOCK_GRASS}, {"dirt", BLOCK_DIRT},
        {"log", BLOCK_LOG}, {"leaves", BLOCK_LEAVES}, {"stone", BLOCK_STONE},
        {"planks", BLOCK_PLANKS}, {"stick", ITEM_STICK},
        {"crafting_table", BLOCK_CRAFTING_TABLE},

        {"diamond_ore", BLOCK_DIAMOND_ORE},
        {"gold_ore", BLOCK_GOLD_ORE},
        {"iron_ore", BLOCK_IRON_ORE},

        {"diamond_block", BLOCK_DIAMOND_BLOCK},
        {"gold_block", BLOCK_GOLD_BLOCK},
        {"iron_block", BLOCK_IRON_BLOCK},

        {"diamond", ITEM_DIAMOND},
        {"gold_ingot", ITEM_GOLD_INGOT},
        {"iron_ingot", ITEM_IRON_INGOT},
        {"gold_nugget", ITEM_GOLD_NUGGET},
        {"iron_nugget", ITEM_IRON_NUGGET},

        {"apple", ITEM_APPLE},
        {"golden_apple", ITEM_GOLDEN_APPLE},

        {"wooden_pickaxe", ITEM_WOODEN_PICKAXE},
        {"wooden_axe", ITEM_WOODEN_AXE},
        {"wooden_shovel", ITEM_WOODEN_SHOVEL},
        {"wooden_sword", ITEM_WOODEN_SWORD},
        {"wooden_hoe", ITEM_WOODEN_HOE},

        {"stone_pickaxe", ITEM_STONE_PICKAXE},
        {"stone_axe", ITEM_STONE_AXE},
        {"stone_shovel", ITEM_STONE_SHOVEL},
        {"stone_sword", ITEM_STONE_SWORD},
        {"stone_hoe", ITEM_STONE_HOE},

        {"iron_pickaxe", ITEM_IRON_PICKAXE},
        {"iron_axe", ITEM_IRON_AXE},
        {"iron_shovel", ITEM_IRON_SHOVEL},
        {"iron_sword", ITEM_IRON_SWORD},
        {"iron_hoe", ITEM_IRON_HOE},

        {"iron_helmet", ITEM_IRON_HELMET},
        {"iron_chestplate", ITEM_IRON_CHESTPLATE},
        {"iron_leggings", ITEM_IRON_LEGGINGS},
        {"iron_boots", ITEM_IRON_BOOTS},

        {"golden_pickaxe", ITEM_GOLDEN_PICKAXE},
        {"golden_axe", ITEM_GOLDEN_AXE},
        {"golden_shovel", ITEM_GOLDEN_SHOVEL},
        {"golden_sword", ITEM_GOLDEN_SWORD},
        {"golden_hoe", ITEM_GOLDEN_HOE},

        {"golden_helmet", ITEM_GOLDEN_HELMET},
        {"golden_chestplate", ITEM_GOLDEN_CHESTPLATE},
        {"golden_leggings", ITEM_GOLDEN_LEGGINGS},
        {"golden_boots", ITEM_GOLDEN_BOOTS},

        {"diamond_pickaxe", ITEM_DIAMOND_PICKAXE},
        {"diamond_axe", ITEM_DIAMOND_AXE},
        {"diamond_shovel", ITEM_DIAMOND_SHOVEL},
        {"diamond_sword", ITEM_DIAMOND_SWORD},
        {"diamond_hoe", ITEM_DIAMOND_HOE},

        {"diamond_helmet", ITEM_DIAMOND_HELMET},
        {"diamond_chestplate", ITEM_DIAMOND_CHESTPLATE},
        {"diamond_leggings", ITEM_DIAMOND_LEGGINGS},
        {"diamond_boots", ITEM_DIAMOND_BOOTS},

        {"cobblestone", BLOCK_COBBLESTONE},
        {"coal_ore", BLOCK_COAL_ORE},
        {"coal", ITEM_COAL},
        {"coal_block", BLOCK_COAL_BLOCK},
        {"furnace", BLOCK_FURNACE},
    };
    auto it = map.find(name);
    return (it != map.end()) ? it->second : BLOCK_AIR;
}

static int parseInt(const std::string &s, size_t &pos)
{
    skipWS(s, pos);
    int val = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
        val = val * 10 + (s[pos++] - '0');
    return val;
}

static void parsePattern(const std::string &s, size_t &pos,
    int pattern[3][3], int &outW, int &outH)
{

    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            pattern[y][x] = BLOCK_AIR;
    outW = 0; outH = 0;

    skipWS(s, pos);
    if (pos >= s.size() || s[pos] != '[') return;
    ++pos;

    int rowCount = 0;
    int maxCols = 0;

    for (int y = 0; y < 3; ++y)
    {
        skipWS(s, pos);
        if (pos >= s.size() || s[pos] != '[') break;
        ++pos;
        ++rowCount;
        skipWS(s, pos);

        int colCount = 0;
        for (int x = 0; x < 3; ++x)
        {
            skipWS(s, pos);
            if (pos >= s.size() || s[pos] == ']') break;

            if (isNull(s, pos))
            {
                pattern[y][x] = BLOCK_AIR;
                pos += 4;
            }
            else
            {
                pattern[y][x] = parseBlockType(parseStr(s, pos));
            }
            ++colCount;

            skipWS(s, pos);
            if (pos < s.size() && s[pos] == ',') ++pos;
        }

        if (colCount > maxCols) maxCols = colCount;

        skipWS(s, pos);
        if (pos < s.size() && s[pos] == ']') ++pos;
        skipWS(s, pos);
        if (pos < s.size() && s[pos] == ',') ++pos;
    }

    skipWS(s, pos);
    if (pos < s.size() && s[pos] == ']') ++pos;

    outH = rowCount;
    outW = maxCols;
    if (outW < 1) outW = 1;
    if (outH < 1) outH = 1;
}

static std::vector<int> parseShapelessBody(const std::string &s, size_t &pos)
{
    std::vector<int> result;
    skipWS(s, pos);
    if (pos >= s.size() || s[pos] != '{') return result;
    ++pos;
    while (pos < s.size() && s[pos] != '}')
    {
        skipWS(s, pos);
        std::string name = parseStr(s, pos);
        skipWS(s, pos);
        if (pos < s.size() && s[pos] == ':') ++pos;
        int cnt = parseInt(s, pos);
        int type = parseBlockType(name);
        for (int i = 0; i < cnt; ++i)
            result.push_back(type);
        skipWS(s, pos);
        if (pos < s.size() && s[pos] == ',') ++pos;
    }
    if (pos < s.size()) ++pos;
    return result;
}

static void loadRecipeFile(const std::string &json, CraftingManager &mgr, int targetType)
{
    size_t pos = 0;
    skipWS(json, pos);
    if (pos >= json.size() || json[pos] != '{') return;
    ++pos;

    while (pos < json.size() && json[pos] != '}')
    {
        skipWS(json, pos);
        std::string key = parseStr(json, pos);
        skipWS(json, pos);
        if (pos < json.size() && json[pos] == ':') ++pos;
        skipWS(json, pos);

        if (pos < json.size() && json[pos] == '[') ++pos;
        skipWS(json, pos);

        while (pos < json.size() && json[pos] != ']')
        {
            skipWS(json, pos);
            if (pos >= json.size() || json[pos] != '{') break;
            ++pos;

            int count = 1;
            int pattern[3][3] = {};
            int recipeW = 0, recipeH = 0;
            std::vector<int> shapelessInputs;

            while (pos < json.size() && json[pos] != '}')
            {
                skipWS(json, pos);
                std::string innerKey = parseStr(json, pos);
                skipWS(json, pos);
                if (pos < json.size() && json[pos] == ':') ++pos;
                skipWS(json, pos);

                if (innerKey == "count")
                    count = parseInt(json, pos);
                else if (innerKey == "recipe")
                {
                    skipWS(json, pos);
                    if (pos < json.size() && json[pos] == '[')
                        parsePattern(json, pos, pattern, recipeW, recipeH);
                    else if (pos < json.size() && json[pos] == '{')
                        shapelessInputs = parseShapelessBody(json, pos);
                }
                skipWS(json, pos);
                if (pos < json.size() && json[pos] == ',') ++pos;
            }
            if (pos < json.size()) ++pos;

            if (key == "shaped")
            {
                bool hasContent = false;
                for (int y = 0; y < recipeH; ++y)
                    for (int x = 0; x < recipeW; ++x)
                        if (pattern[y][x] != BLOCK_AIR) hasContent = true;
                if (hasContent) mgr.addShaped(pattern, recipeW, recipeH, targetType, count);
            }
            else if (key == "shapeless" && !shapelessInputs.empty())
                mgr.addShapeless(shapelessInputs, targetType, count);

            skipWS(json, pos);
            if (pos < json.size() && json[pos] == ',') ++pos;
        }
        if (pos < json.size()) ++pos;
        skipWS(json, pos);
        if (pos < json.size() && json[pos] == ',') ++pos;
    }
}

static int blockTypeFromFilename(const char *filename)
{
    std::string name(filename);
    auto dot = name.find('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return parseBlockType(name);
}

static void loadAllRecipes(CraftingManager &mgr)
{
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(L"../assert/recipes/*.json", &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {

        char filename[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, filename, 256, nullptr, nullptr);

        std::string path = "../assert/recipes/";
        path += filename;

        int targetType = blockTypeFromFilename(filename);
        if (targetType == BLOCK_AIR) continue;

        std::string json = readFile(path.c_str());
        if (!json.empty())
            loadRecipeFile(json, mgr, targetType);

    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

CraftingManager::CraftingManager()
{
    loadAllRecipes(*this);
}

void CraftingManager::addShapeless(const std::vector<int> &inputs,
    int outputType, int outputCount)
{
    ShapelessRecipe r;
    r.inputs = inputs;
    r.outputType = outputType;
    r.outputCount = outputCount;
    m_shapeless.push_back(r);
}

void CraftingManager::addShaped(const int pattern[3][3],
    int w, int h, int outputType, int outputCount)
{
    ShapedRecipe r;
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            r.pattern[y][x] = pattern[y][x];
    r.recipeW = w;
    r.recipeH = h;
    r.outputType = outputType;
    r.outputCount = outputCount;
    m_shaped.push_back(r);
}

std::unordered_map<int, int> countItems3x3(const int grid[3][3])
{
    std::unordered_map<int, int> cnt;
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
        {
            int t = grid[y][x];
            if (t != BLOCK_AIR) ++cnt[t];
        }
    return cnt;
}

std::unordered_map<int, int> countItemsVec(const std::vector<int> &items)
{
    std::unordered_map<int, int> cnt;
    for (int t : items)
        if (t != BLOCK_AIR) ++cnt[t];
    return cnt;
}

CraftResult CraftingManager::match(const int grid[3][3]) const
{
    CraftResult result;

    for (const auto &r : m_shaped)
    {
        int maxOffY = 3 - r.recipeH;
        int maxOffX = 3 - r.recipeW;

        for (int offY = 0; offY <= maxOffY; ++offY)
        {
            for (int offX = 0; offX <= maxOffX; ++offX)
            {
                bool ok = true;

                for (int y = 0; y < r.recipeH && ok; ++y)
                    for (int x = 0; x < r.recipeW && ok; ++x)
                        if (grid[offY + y][offX + x] != r.pattern[y][x])
                            ok = false;

                for (int y = 0; y < 3 && ok; ++y)
                    for (int x = 0; x < 3 && ok; ++x)
                    {
                        bool inRecipe = (y >= offY && y < offY + r.recipeH &&
                            x >= offX && x < offX + r.recipeW);
                        if (!inRecipe && grid[y][x] != BLOCK_AIR)
                            ok = false;
                    }

                if (ok)
                {
                    result.valid = true;
                    result.outputType = r.outputType;
                    result.outputCount = r.outputCount;
                    result.isShaped = true;
                    return result;
                }
            }
        }
    }

    auto gridCnt = countItems3x3(grid);
    for (const auto &r : m_shapeless)
    {
        auto needCnt = countItemsVec(r.inputs);

        bool ok = true;
        for (const auto &[type, need] : needCnt)
        {
            auto it = gridCnt.find(type);
            int have = (it != gridCnt.end()) ? it->second : 0;
            if (have < need) { ok = false; break; }
        }

        if (ok)
        {
            for (const auto &[type, have] : gridCnt)
            {
                auto it = needCnt.find(type);
                int need = (it != needCnt.end()) ? it->second : 0;
                if (have > need) { ok = false; break; }
            }
        }

        if (ok)
        {
            result.valid = true;
            result.outputType = r.outputType;
            result.outputCount = r.outputCount;
            result.isShaped = false;
            return result;
        }
    }

    return result;
}