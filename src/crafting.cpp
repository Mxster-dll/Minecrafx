#include "crafting.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <windows.h>

// ============================================================================
// 简易 JSON 解析器（支持新配方格式）
// ============================================================================

static std::string readFile(const char *path)
{
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
    static std::unordered_map<std::string, int> map = {
        {"air", BLOCK_AIR}, {"grass", BLOCK_GRASS}, {"dirt", BLOCK_DIRT},
        {"log", BLOCK_LOG}, {"leaves", BLOCK_LEAVES}, {"stone", BLOCK_STONE},
        {"planks", BLOCK_PLANKS}, {"stick", BLOCK_STICK},
        {"crafting_table", BLOCK_CRAFTING_TABLE}
    };
    auto it = map.find(name);
    return (it != map.end()) ? it->second : BLOCK_AIR;
}

// 解析整数
static int parseInt(const std::string &s, size_t &pos)
{
    skipWS(s, pos);
    int val = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
        val = val * 10 + (s[pos++] - '0');
    return val;
}

// 解析 3×3 模式：[[...],[...],[...]]
static void parsePattern3x3(const std::string &s, size_t &pos, int pattern[3][3])
{
    skipWS(s, pos);
    if (pos >= s.size() || s[pos] != '[') return;
    ++pos;  // outer [
    for (int y = 0; y < 3; ++y)
    {
        skipWS(s, pos);
        if (pos >= s.size() || s[pos] != '[') break;
        ++pos;  // inner [
        skipWS(s, pos);
        for (int x = 0; x < 3; ++x)
        {
            skipWS(s, pos);
            if (isNull(s, pos))
            {
                pattern[y][x] = BLOCK_AIR;
                pos += 4;
            }
            else
                pattern[y][x] = parseBlockType(parseStr(s, pos));
            skipWS(s, pos);
            if (pos < s.size() && s[pos] == ',') ++pos;
        }
        skipWS(s, pos);
        if (pos < s.size() && s[pos] == ']') ++pos;
        skipWS(s, pos);
        if (pos < s.size() && s[pos] == ',') ++pos;
    }
    skipWS(s, pos);
    if (pos < s.size() && s[pos] == ']') ++pos;
}

// 解析 shapeless 配方主体：{ "dirt": 9, "stone": 1 } → vector<int> (展平)
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
    if (pos < s.size()) ++pos;  // }
    return result;
}

// 解析单个配方文件的 JSON，加载到 mgr。targetType 由文件名决定
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
                        parsePattern3x3(json, pos, pattern);
                    else if (pos < json.size() && json[pos] == '{')
                        shapelessInputs = parseShapelessBody(json, pos);
                }
                skipWS(json, pos);
                if (pos < json.size() && json[pos] == ',') ++pos;
            }
            if (pos < json.size()) ++pos;  // }

            if (key == "shaped")
            {
                bool hasContent = false;
                for (int y = 0; y < 3; ++y)
                    for (int x = 0; x < 3; ++x)
                        if (pattern[y][x] != BLOCK_AIR) hasContent = true;
                if (hasContent) mgr.addShaped(pattern, targetType, count);
            }
            else if (key == "shapeless" && !shapelessInputs.empty())
                mgr.addShapeless(shapelessInputs, targetType, count);

            skipWS(json, pos);
            if (pos < json.size() && json[pos] == ',') ++pos;
        }
        if (pos < json.size()) ++pos;  // ]
        skipWS(json, pos);
        if (pos < json.size() && json[pos] == ',') ++pos;
    }
}

// 从文件名提取方块类型 (例如 "planks.json" → BLOCK_PLANKS)
static int blockTypeFromFilename(const char *filename)
{
    std::string name(filename);
    auto dot = name.find('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return parseBlockType(name);
}

// 扫描文件夹加载所有配方
static void loadAllRecipes(CraftingManager &mgr)
{
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(L"../assert/recipes/*.json", &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {
        // 宽字符转 UTF-8
        char filename[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, filename, 256, nullptr, nullptr);

        std::string path = "../assert/recipes/";
        path += filename;

        int targetType = blockTypeFromFilename(filename);
        if (targetType == BLOCK_AIR) continue;  // 未知类型跳过

        std::string json = readFile(path.c_str());
        if (!json.empty())
            loadRecipeFile(json, mgr, targetType);

    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

// ============================================================================
// CraftingManager
// ============================================================================

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
    int outputType, int outputCount)
{
    ShapedRecipe r;
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            r.pattern[y][x] = pattern[y][x];
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

    // ---- 优先匹配有序配方 ----
    for (const auto &r : m_shaped)
    {
        bool ok = true;
        for (int y = 0; y < 3 && ok; ++y)
            for (int x = 0; x < 3 && ok; ++x)
            {
                if (grid[y][x] != r.pattern[y][x])
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

    // ---- 再匹配无形状配方 ----
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
