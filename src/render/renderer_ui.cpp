#include "renderer.h"
#include "../core/constant.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <functional>
#include <windows.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>


// ============================================================================
// 此文件包含 HUD、准星、背景模糊和后处理、按钮等 GUI 方法
// ============================================================================

// ============================================================================
// 此文件包含 Renderer 类的所有 UI / 纹理 / GUI 方法
// 从 renderer.cpp 拆分出来以减少单文件体积
// ============================================================================

// ============================================================================
// Alpha 混合：EasyX PNG 像素 (0xAARRGGBB) → DIB 像素 (0x00RRGGBB)
// ============================================================================

/** @brief 将 ARGB 源像素叠加到 DIB 目标像素，返回混合结果 */
static inline DWORD alphaBlend(DWORD dst, DWORD src)
{
    unsigned int a = (src >> 24) & 0xFF;
    if (a == 0) return dst;
    if (a == 255) return src & 0x00FFFFFF;
    unsigned int sr = (src >> 16) & 0xFF;
    unsigned int sg = (src >> 8) & 0xFF;
    unsigned int sb = src & 0xFF;
    unsigned int dr = (dst >> 16) & 0xFF;
    unsigned int dg = (dst >> 8) & 0xFF;
    unsigned int db = dst & 0xFF;
    unsigned int invA = 255 - a;
    dr = (sr * a + dr * invA) / 255;
    dg = (sg * a + dg * invA) / 255;
    db = (sb * a + db * invA) / 255;
    return RGB(dr, dg, db);
}

// ============================================================================
// 方块贴图加载（像素数据）
// ============================================================================

static void loadTexPixels(const wchar_t *path, COLORREF out[16][16], int &w, int &h)
{
    w = h = 0;
    IMAGE img;
    loadimage(&img, path);
    DWORD *buf = GetImageBuffer(&img);
    if (!buf) return;
    w = img.getwidth(); h = img.getheight();
    if (w <= 0 || h <= 0) { w = h = 0; return; }
    if (w > 16) w = 16;
    if (h > 16) h = 16;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            out[y][x] = buf[y * img.getwidth() + x];
}

void Renderer::loadBlockTextures()
{
    int tw, th;
    // 草方块 0-2
    loadTexPixels(L"../assert/texture/grass_block_top.png", m_texPixels[0], tw, th); m_texW[0] = tw; m_texH[0] = th;
    loadTexPixels(L"../assert/texture/grass_block_side.png", m_texPixels[1], tw, th); m_texW[1] = tw; m_texH[1] = th;
    loadTexPixels(L"../assert/texture/grass_block_bottom.png", m_texPixels[2], tw, th); m_texW[2] = tw; m_texH[2] = th;
    // 泥土 3-5
    loadTexPixels(L"../assert/texture/dirt_top.png", m_texPixels[3], tw, th); m_texW[3] = tw; m_texH[3] = th;
    loadTexPixels(L"../assert/texture/dirt_side.png", m_texPixels[4], tw, th); m_texW[4] = tw; m_texH[4] = th;
    loadTexPixels(L"../assert/texture/dirt_bottom.png", m_texPixels[5], tw, th); m_texW[5] = tw; m_texH[5] = th;
    // 树干 6-8
    loadTexPixels(L"../assert/texture/oak_log_top.png", m_texPixels[6], tw, th); m_texW[6] = tw; m_texH[6] = th;
    loadTexPixels(L"../assert/texture/oak_log_side.png", m_texPixels[7], tw, th); m_texW[7] = tw; m_texH[7] = th;
    loadTexPixels(L"../assert/texture/oak_log_bottom.png", m_texPixels[8], tw, th); m_texW[8] = tw; m_texH[8] = th;
    // 树叶 9-11
    loadTexPixels(L"../assert/texture/oak_leaves_top.png", m_texPixels[9], tw, th); m_texW[9] = tw; m_texH[9] = th;
    loadTexPixels(L"../assert/texture/oak_leaves_side.png", m_texPixels[10], tw, th); m_texW[10] = tw; m_texH[10] = th;
    loadTexPixels(L"../assert/texture/oak_leaves_bottom.png", m_texPixels[11], tw, th); m_texW[11] = tw; m_texH[11] = th;
    // 石头 12-14
    loadTexPixels(L"../assert/texture/stone_top.png", m_texPixels[12], tw, th); m_texW[12] = tw; m_texH[12] = th;
    loadTexPixels(L"../assert/texture/stone_side.png", m_texPixels[13], tw, th); m_texW[13] = tw; m_texH[13] = th;
    loadTexPixels(L"../assert/texture/stone_bottom.png", m_texPixels[14], tw, th); m_texW[14] = tw; m_texH[14] = th;
    // 木板 15-17
    loadTexPixels(L"../assert/texture/oak_planks_top.png", m_texPixels[15], tw, th); m_texW[15] = tw; m_texH[15] = th;
    loadTexPixels(L"../assert/texture/oak_planks_side.png", m_texPixels[16], tw, th); m_texW[16] = tw; m_texH[16] = th;
    loadTexPixels(L"../assert/texture/oak_planks_bottom.png", m_texPixels[17], tw, th); m_texW[17] = tw; m_texH[17] = th;
    // 木棍 18-20（无方块贴图）
    // 工作台 21-23
    loadTexPixels(L"../assert/texture/crafting_table_top.png", m_texPixels[21], tw, th); m_texW[21] = tw; m_texH[21] = th;
    loadTexPixels(L"../assert/texture/crafting_table_front.png", m_texPixels[22], tw, th); m_texW[22] = tw; m_texH[22] = th;
    loadTexPixels(L"../assert/texture/crafting_table_bottom.png", m_texPixels[23], tw, th); m_texW[23] = tw; m_texH[23] = th;
    // 钻石矿 24-26
    loadTexPixels(L"../assert/texture/diamond_ore_top.png", m_texPixels[24], tw, th); m_texW[24] = tw; m_texH[24] = th;
    loadTexPixels(L"../assert/texture/diamond_ore_side.png", m_texPixels[25], tw, th); m_texW[25] = tw; m_texH[25] = th;
    loadTexPixels(L"../assert/texture/diamond_ore_bottom.png", m_texPixels[26], tw, th); m_texW[26] = tw; m_texH[26] = th;
    // 金矿 27-29
    loadTexPixels(L"../assert/texture/gold_ore_top.png", m_texPixels[27], tw, th); m_texW[27] = tw; m_texH[27] = th;
    loadTexPixels(L"../assert/texture/gold_ore_side.png", m_texPixels[28], tw, th); m_texW[28] = tw; m_texH[28] = th;
    loadTexPixels(L"../assert/texture/gold_ore_bottom.png", m_texPixels[29], tw, th); m_texW[29] = tw; m_texH[29] = th;
    // 铁矿 30-32
    loadTexPixels(L"../assert/texture/iron_ore_top.png", m_texPixels[30], tw, th); m_texW[30] = tw; m_texH[30] = th;
    loadTexPixels(L"../assert/texture/iron_ore_side.png", m_texPixels[31], tw, th); m_texW[31] = tw; m_texH[31] = th;
    loadTexPixels(L"../assert/texture/iron_ore_bottom.png", m_texPixels[32], tw, th); m_texW[32] = tw; m_texH[32] = th;
    // 钻石块 33-35
    loadTexPixels(L"../assert/texture/diamond_block_top.png", m_texPixels[33], tw, th); m_texW[33] = tw; m_texH[33] = th;
    loadTexPixels(L"../assert/texture/diamond_block_side.png", m_texPixels[34], tw, th); m_texW[34] = tw; m_texH[34] = th;
    loadTexPixels(L"../assert/texture/diamond_block_bottom.png", m_texPixels[35], tw, th); m_texW[35] = tw; m_texH[35] = th;
    // 金块 36-38
    loadTexPixels(L"../assert/texture/gold_block_top.png", m_texPixels[36], tw, th); m_texW[36] = tw; m_texH[36] = th;
    loadTexPixels(L"../assert/texture/gold_block_side.png", m_texPixels[37], tw, th); m_texW[37] = tw; m_texH[37] = th;
    loadTexPixels(L"../assert/texture/gold_block_bottom.png", m_texPixels[38], tw, th); m_texW[38] = tw; m_texH[38] = th;
    // 铁块 39-41
    loadTexPixels(L"../assert/texture/iron_block_top.png", m_texPixels[39], tw, th); m_texW[39] = tw; m_texH[39] = th;
    loadTexPixels(L"../assert/texture/iron_block_side.png", m_texPixels[40], tw, th); m_texW[40] = tw; m_texH[40] = th;
    loadTexPixels(L"../assert/texture/iron_block_bottom.png", m_texPixels[41], tw, th); m_texW[41] = tw; m_texH[41] = th;
    // 圆石 42-44
    loadTexPixels(L"../assert/texture/cobblestone_top.png", m_texPixels[42], tw, th); m_texW[42] = tw; m_texH[42] = th;
    loadTexPixels(L"../assert/texture/cobblestone_side.png", m_texPixels[43], tw, th); m_texW[43] = tw; m_texH[43] = th;
    loadTexPixels(L"../assert/texture/cobblestone_bottom.png", m_texPixels[44], tw, th); m_texW[44] = tw; m_texH[44] = th;
    // 煤矿 45-47
    loadTexPixels(L"../assert/texture/coal_ore_top.png", m_texPixels[45], tw, th); m_texW[45] = tw; m_texH[45] = th;
    loadTexPixels(L"../assert/texture/coal_ore_side.png", m_texPixels[46], tw, th); m_texW[46] = tw; m_texH[46] = th;
    loadTexPixels(L"../assert/texture/coal_ore_bottom.png", m_texPixels[47], tw, th); m_texW[47] = tw; m_texH[47] = th;
    // 煤炭块 48-50
    loadTexPixels(L"../assert/texture/coal_block_top.png", m_texPixels[48], tw, th); m_texW[48] = tw; m_texH[48] = th;
    loadTexPixels(L"../assert/texture/coal_block_side.png", m_texPixels[49], tw, th); m_texW[49] = tw; m_texH[49] = th;
    loadTexPixels(L"../assert/texture/coal_block_bottom.png", m_texPixels[50], tw, th); m_texW[50] = tw; m_texH[50] = th;
    // 熔炉 51-54 (top, side, side_on, bottom — front 用 side)
    loadTexPixels(L"../assert/texture/furnace_top.png", m_texPixels[51], tw, th); m_texW[51] = tw; m_texH[51] = th;
    loadTexPixels(L"../assert/texture/furnace_side.png", m_texPixels[52], tw, th); m_texW[52] = tw; m_texH[52] = th;
    loadTexPixels(L"../assert/texture/furnace_bottom.png", m_texPixels[53], tw, th); m_texW[53] = tw; m_texH[53] = th;
    loadTexPixels(L"../assert/texture/furnace_side_on.png", m_texPixels[54], tw, th); m_texW[54] = tw; m_texH[54] = th;

    m_blockTexLoaded = true;
}

int Renderer::blockTexId(int blockType, int face)
{
    // 只有可放置的实体方块才有贴图
    switch (blockType)
    {
        case BLOCK_GRASS:           return 0 + face;
        case BLOCK_DIRT:            return 3 + face;
        case BLOCK_LOG:             return 6 + face;
        case BLOCK_LEAVES:          return 9 + face;
        case BLOCK_STONE:           return 12 + face;
        case BLOCK_PLANKS:          return 15 + face;
        case BLOCK_CRAFTING_TABLE:  return 21 + face;
        case BLOCK_DIAMOND_ORE:     return 24 + face;
        case BLOCK_GOLD_ORE:        return 27 + face;
        case BLOCK_IRON_ORE:        return 30 + face;
        case BLOCK_DIAMOND_BLOCK:   return 33 + face;
        case BLOCK_GOLD_BLOCK:      return 36 + face;
        case BLOCK_IRON_BLOCK:      return 39 + face;
        case BLOCK_COBBLESTONE:     return 42 + face;
        case BLOCK_COAL_ORE:        return 45 + face;
        case BLOCK_COAL_BLOCK:      return 48 + face;
        case BLOCK_FURNACE:
            // 侧面：活跃时显示 furnace_side_on
            if (face == 1 && m_furnaceActive) return 54;
            return 51 + face;
        default: return -1;
    }
}

void Renderer::loadDestroyStages()
{
    wchar_t path[128];
    for (int i = 0; i < DESTROY_STAGES; ++i)
    {
        swprintf(path, 128, L"../assert/texture/destroy_stage_%d.png", i);
        int w, h;
        loadTexPixels(path, m_destroyPixels[i], w, h);
    }
    m_destroyLoaded = true;
}

COLORREF Renderer::sampleTexture(int texId, double tu, double tv) const
{
    if (texId < 0 || texId >= MAX_TEX || !m_blockTexLoaded)
        return RGB(128, 128, 128);
    int w = m_texW[texId], h = m_texH[texId];
    if (w <= 0 || h <= 0) return RGB(128, 128, 128);
    tu = tu - std::floor(tu);
    tv = tv - std::floor(tv);
    if (tu < 0) tu += 1.0;
    if (tv < 0) tv += 1.0;
    int px = (int) (tu * w);
    int py = (int) (tv * h);
    if (px < 0) px = 0;
    if (px >= w) px = w - 1;
    if (py < 0) py = 0;
    if (py >= h) py = h - 1;
    return m_texPixels[texId][py][px];
}


// ============================================================================
// 热键栏
// ============================================================================

// 图标路径数组，索引 = blockType（1~60），0 未使用
static const wchar_t *kHotbarIcons[MAX_BLOCK_TYPE] = {
    L"",  // 0: AIR
    L"../assert/gui/item/grass_block.png",        // 1
    L"../assert/gui/item/dirt.png",                // 2
    L"../assert/gui/item/oak_log.png",             // 3
    L"../assert/gui/item/oak_leaves.png",          // 4
    L"../assert/gui/item/stone.png",               // 5
    L"../assert/gui/item/oak_planks.png",          // 6
    L"../assert/gui/item/stick.png",               // 7
    L"../assert/gui/item/crafting_table.png",      // 8
    L"../assert/gui/item/diamond_ore.png",         // 9
    L"../assert/gui/item/gold_ore.png",            // 10
    L"../assert/gui/item/iron_ore.png",            // 11
    L"../assert/gui/item/diamond_block.png",       // 12
    L"../assert/gui/item/gold_block.png",          // 13
    L"../assert/gui/item/iron_block.png",          // 14
    L"../assert/gui/item/diamond.png",             // 15
    L"../assert/gui/item/gold_ingot.png",          // 16
    L"../assert/gui/item/iron_ingot.png",          // 17
    L"../assert/gui/item/gold_nugget.png",         // 18
    L"../assert/gui/item/iron_nugget.png",         // 19
    L"../assert/gui/item/apple.png",               // 20
    L"../assert/gui/item/golden_apple.png",        // 21
    L"../assert/gui/item/wooden_pickaxe.png",      // 22
    L"../assert/gui/item/wooden_axe.png",          // 23
    L"../assert/gui/item/wooden_shovel.png",       // 24
    L"../assert/gui/item/wooden_sword.png",        // 25
    L"../assert/gui/item/wooden_hoe.png",          // 26
    L"../assert/gui/item/stone_pickaxe.png",       // 27
    L"../assert/gui/item/stone_axe.png",           // 28
    L"../assert/gui/item/stone_shovel.png",        // 29
    L"../assert/gui/item/stone_sword.png",         // 30
    L"../assert/gui/item/stone_hoe.png",           // 31
    L"../assert/gui/item/iron_pickaxe.png",        // 32
    L"../assert/gui/item/iron_axe.png",            // 33
    L"../assert/gui/item/iron_shovel.png",         // 34
    L"../assert/gui/item/iron_sword.png",          // 35
    L"../assert/gui/item/iron_hoe.png",            // 36
    L"../assert/gui/item/iron_helmet.png",         // 37
    L"../assert/gui/item/iron_chestplate.png",     // 38
    L"../assert/gui/item/iron_leggings.png",       // 39
    L"../assert/gui/item/iron_boots.png",          // 40
    L"../assert/gui/item/golden_pickaxe.png",      // 41
    L"../assert/gui/item/golden_axe.png",          // 42
    L"../assert/gui/item/golden_shovel.png",       // 43
    L"../assert/gui/item/golden_sword.png",        // 44
    L"../assert/gui/item/golden_hoe.png",          // 45
    L"../assert/gui/item/golden_helmet.png",       // 46
    L"../assert/gui/item/golden_chestplate.png",   // 47
    L"../assert/gui/item/golden_leggings.png",     // 48
    L"../assert/gui/item/golden_boots.png",        // 49
    L"../assert/gui/item/diamond_pickaxe.png",     // 50
    L"../assert/gui/item/diamond_axe.png",         // 51
    L"../assert/gui/item/diamond_shovel.png",      // 52
    L"../assert/gui/item/diamond_sword.png",       // 53
    L"../assert/gui/item/diamond_hoe.png",         // 54
    L"../assert/gui/item/diamond_helmet.png",      // 55
    L"../assert/gui/item/diamond_chestplate.png",  // 56
    L"../assert/gui/item/diamond_leggings.png",    // 57
    L"../assert/gui/item/diamond_boots.png",       // 58
    L"../assert/gui/item/cobblestone.png",        // 59
    L"../assert/gui/item/coal_ore.png",           // 60
    L"../assert/gui/item/coal.png",               // 61
    L"../assert/gui/item/coal_block.png",         // 62
    L"../assert/gui/item/furnace.png",            // 63
};

static const wchar_t *kBigIconPaths[MAX_BLOCK_TYPE] = {
    L"", L"../assert/gui/item/grass_block.png", L"../assert/gui/item/dirt.png",
    L"../assert/gui/item/oak_log.png", L"../assert/gui/item/oak_leaves.png",
    L"../assert/gui/item/stone.png", L"../assert/gui/item/oak_planks.png",
    L"../assert/gui/item/stick.png", L"../assert/gui/item/crafting_table.png",
    L"../assert/gui/item/diamond_ore.png", L"../assert/gui/item/gold_ore.png",
    L"../assert/gui/item/iron_ore.png", L"../assert/gui/item/diamond_block.png",
    L"../assert/gui/item/gold_block.png", L"../assert/gui/item/iron_block.png",
    L"../assert/gui/item/diamond.png", L"../assert/gui/item/gold_ingot.png",
    L"../assert/gui/item/iron_ingot.png", L"../assert/gui/item/gold_nugget.png",
    L"../assert/gui/item/iron_nugget.png", L"../assert/gui/item/apple.png",
    L"../assert/gui/item/golden_apple.png",
    L"../assert/gui/item/wooden_pickaxe.png", L"../assert/gui/item/wooden_axe.png",
    L"../assert/gui/item/wooden_shovel.png", L"../assert/gui/item/wooden_sword.png",
    L"../assert/gui/item/wooden_hoe.png",
    L"../assert/gui/item/stone_pickaxe.png", L"../assert/gui/item/stone_axe.png",
    L"../assert/gui/item/stone_shovel.png", L"../assert/gui/item/stone_sword.png",
    L"../assert/gui/item/stone_hoe.png",
    L"../assert/gui/item/iron_pickaxe.png", L"../assert/gui/item/iron_axe.png",
    L"../assert/gui/item/iron_shovel.png", L"../assert/gui/item/iron_sword.png",
    L"../assert/gui/item/iron_hoe.png",
    L"../assert/gui/item/iron_helmet.png", L"../assert/gui/item/iron_chestplate.png",
    L"../assert/gui/item/iron_leggings.png", L"../assert/gui/item/iron_boots.png",
    L"../assert/gui/item/golden_pickaxe.png", L"../assert/gui/item/golden_axe.png",
    L"../assert/gui/item/golden_shovel.png", L"../assert/gui/item/golden_sword.png",
    L"../assert/gui/item/golden_hoe.png",
    L"../assert/gui/item/golden_helmet.png", L"../assert/gui/item/golden_chestplate.png",
    L"../assert/gui/item/golden_leggings.png", L"../assert/gui/item/golden_boots.png",
    L"../assert/gui/item/diamond_pickaxe.png", L"../assert/gui/item/diamond_axe.png",
    L"../assert/gui/item/diamond_shovel.png", L"../assert/gui/item/diamond_sword.png",
    L"../assert/gui/item/diamond_hoe.png",
    L"../assert/gui/item/diamond_helmet.png", L"../assert/gui/item/diamond_chestplate.png",
    L"../assert/gui/item/diamond_leggings.png", L"../assert/gui/item/diamond_boots.png",
    L"../assert/gui/item/cobblestone.png",       // 59
    L"../assert/gui/item/coal_ore.png",          // 60
    L"../assert/gui/item/coal.png",              // 61
    L"../assert/gui/item/coal_block.png",        // 62
    L"../assert/gui/item/furnace.png",           // 63
};

void Renderer::loadHotbar()
{
    bool bgOk = false;
    // 加载 hotbar 背景
    {
        IMAGE img;
        loadimage(&img, L"../assert/gui/widget/hotbar.png");
        DWORD *buf = GetImageBuffer(&img);
        int srcW = img.getwidth();
        if (buf && srcW > 0)
        {
            m_hbBgW = img.getwidth(); m_hbBgH = img.getheight();
            m_hotbarBg.resize(m_hbBgW * m_hbBgH);
            for (int y = 0; y < m_hbBgH; ++y)
                for (int x = 0; x < m_hbBgW; ++x)
                    m_hotbarBg[y * m_hbBgW + x] = buf[y * srcW + x];
            bgOk = true;
        }
    }
    // 加载快捷栏图标（用 loadimage 缩放至槽位显示尺寸）
    double scale = (double) HB_HEIGHT / m_hbBgH;
    int iconSz = (int) (HB_SLOT_SIZE * scale);
    if (iconSz < 1) iconSz = 1;
    m_hbIconDisplaySize = iconSz;

    for (int i = 1; i < MAX_BLOCK_TYPE; ++i)
    {
        if (kHotbarIcons[i][0] == L'\0') continue;
        IMAGE imgNative;
        loadimage(&imgNative, kHotbarIcons[i]);
        DWORD *nbuf = GetImageBuffer(&imgNative);
        int nw = imgNative.getwidth(), nh = imgNative.getheight();
        if (nbuf && nw > 0 && nh > 0)
        {
            m_hotbarIcons[i].resize(iconSz * iconSz);
            for (int y = 0; y < iconSz; ++y)
            {
                int sy = y * nh / iconSz;
                for (int x = 0; x < iconSz; ++x)
                {
                    int sx = x * nw / iconSz;
                    m_hotbarIcons[i][y * iconSz + x] = nbuf[sy * nw + sx];
                }
            }
        }
    }
    // 加载大图标（最邻近采样至 32x32）
    const int BIG = HB_ICON_SIZE * 2;
    for (int i = 1; i < MAX_BLOCK_TYPE; ++i)
    {
        if (kBigIconPaths[i][0] == L'\0') continue;
        IMAGE imgNative;
        loadimage(&imgNative, kBigIconPaths[i]);
        DWORD *nbuf = GetImageBuffer(&imgNative);
        int nw = imgNative.getwidth(), nh = imgNative.getheight();
        if (nbuf && nw > 0 && nh > 0)
        {
            m_hotbarIconsBig[i].resize(BIG * BIG);
            for (int y = 0; y < BIG; ++y)
            {
                int sy = y * nh / BIG;
                for (int x = 0; x < BIG; ++x)
                {
                    int sx = x * nw / BIG;
                    m_hotbarIconsBig[i][y * BIG + x] = nbuf[sy * nw + sx];
                }
            }
        }
    }
    m_hotbarBlockTypes[0] = BLOCK_GRASS;
    m_hotbarBlockTypes[1] = BLOCK_DIRT;
    m_hotbarBlockTypes[2] = BLOCK_LOG;
    m_hotbarBlockTypes[3] = BLOCK_LEAVES;
    m_hotbarBlockTypes[4] = BLOCK_STONE;
    m_hotbarBlockTypes[5] = BLOCK_PLANKS;
    m_hotbarBlockTypes[6] = BLOCK_AIR;
    m_hotbarBlockTypes[7] = BLOCK_AIR;
    m_hotbarBlockTypes[8] = BLOCK_AIR;

    // 加载选中框 select.png（与 hotbar 同缩放比）
    {
        IMAGE selImg;
        loadimage(&selImg, L"../assert/gui/widget/select.png");
        int selW = selImg.getwidth(), selH = selImg.getheight();
        if (selW > 0 && selH > 0)
        {
            // 按 hotbar 缩放比缩放至显示尺寸
            int dispW = (int) (selW * scale);
            int dispH = (int) (selH * scale);
            if (dispW < 1) dispW = 1;
            if (dispH < 1) dispH = 1;
            loadimage(&selImg, L"../assert/gui/widget/select.png", dispW, dispH, true);
            DWORD *buf = GetImageBuffer(&selImg);
            int srcW = selImg.getwidth();
            if (buf && srcW > 0)
            {
                m_selectW = dispW; m_selectH = dispH;
                m_selectPixels.resize(dispW * dispH);
                for (int y = 0; y < dispH; ++y)
                    for (int x = 0; x < dispW; ++x)
                        m_selectPixels[y * dispW + x] = buf[y * srcW + x];
            }
        }
    }

    m_hotbarLoaded = bgOk;  // 仅背景加载成功才允许绘制
}

void Renderer::loadInventoryIcons()
{
    for (int i = 1; i < MAX_BLOCK_TYPE; ++i)
    {
        if (kHotbarIcons[i][0] == L'\0') continue;
        IMAGE imgNative;
        loadimage(&imgNative, kHotbarIcons[i]);
        DWORD *nbuf = GetImageBuffer(&imgNative);
        int nw = imgNative.getwidth(), nh = imgNative.getheight();
        if (nbuf && nw > 0 && nh > 0)
        {
            m_invIconW[i] = nw;
            m_invIconH[i] = nh;
            m_invIcons[i].resize(nw * nh);
            for (int y = 0; y < nh; ++y)
                for (int x = 0; x < nw; ++x)
                    m_invIcons[i][y * nw + x] = nbuf[y * nw + x];
        }
    }
}

void Renderer::drawBlockIcon(int screenX, int screenY, int size, int blockType, int count)
{
    if (blockType <= 0 || blockType >= MAX_BLOCK_TYPE) return;
    const auto &pixels = m_invIcons[blockType];
    int srcW = m_invIconW[blockType];
    int srcH = m_invIconH[blockType];
    if (pixels.empty() || srcW <= 0 || srcH <= 0) return;

    // 直接缩放：原生分辨率 → 显示尺寸（最邻近采样）
    for (int dy = 0; dy < size; ++dy)
    {
        int py = screenY + dy;
        if (py < 0 || py >= m_screenHeight) continue;
        int sy = dy * srcH / size;
        if (sy >= srcH) sy = srcH - 1;
        int dstRow = py * m_screenWidth;
        for (int dx = 0; dx < size; ++dx)
        {
            int px = screenX + dx;
            if (px < 0 || px >= m_screenWidth) continue;
            int sx = dx * srcW / size;
            if (sx >= srcW) sx = srcW - 1;
            COLORREF c = pixels[sy * srcW + sx];
            if (c == 0) continue;
            m_pBits[dstRow + px] = alphaBlend(m_pBits[dstRow + px], c);
        }
    }

    // 数量文字（右下角）
    if (count > 1)
    {
        wchar_t buf[8];
        swprintf(buf, 8, L"%d", count);
        SetBkMode(m_memDC, TRANSPARENT);
        SetTextColor(m_memDC, RGB(255, 255, 255));
        // 使用小号字体（临时切换到更小的字号）
        HFONT smallFont = CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Minecraft AE");
        HFONT oldF = (HFONT) SelectObject(m_memDC, smallFont);
        SIZE ts;
        GetTextExtentPoint32W(m_memDC, buf, (int) wcslen(buf), &ts);
        TextOutW(m_memDC, screenX + size - ts.cx - 1, screenY + size - ts.cy + 1, buf, (int) wcslen(buf));
        SelectObject(m_memDC, oldF);
        DeleteObject(smallFont);
    }
}

void Renderer::drawHotbar(int selectedSlot, const int *hotbarBlockTypes, const int *hotbarCounts)
{
    if (!m_hotbarLoaded || !m_dibReady || m_hbBgW <= 0 || m_hbBgH <= 0) return;

    // 使用传入的方块类型，否则回退到硬编码
    const int *types = hotbarBlockTypes ? hotbarBlockTypes : m_hotbarBlockTypes;

    // 按高度 44 缩放，宽度自适应（保持比例）
    double scale = (double) HB_HEIGHT / m_hbBgH;
    int hbW = (int) (m_hbBgW * scale);
    int hbH = HB_HEIGHT;
    int hbX = (m_screenWidth - hbW) / 2;
    int hbY = m_screenHeight - hbH - 4;

    // 绘制 hotbar 背景
    for (int dy = 0; dy < hbH; ++dy)
    {
        int sy = (int) (dy / scale);
        if (sy >= m_hbBgH) sy = m_hbBgH - 1;
        for (int dx = 0; dx < hbW; ++dx)
        {
            int sx = (int) (dx / scale);
            if (sx >= m_hbBgW) sx = m_hbBgW - 1;
            COLORREF c = m_hotbarBg[sy * m_hbBgW + sx];
            if (c == 0 || c == RGB(0, 0, 0)) continue;
            int px = hbX + dx, py = hbY + dy;
            if (px >= 0 && px < m_screenWidth && py >= 0 && py < m_screenHeight)
                m_pBits[py * m_screenWidth + px] = alphaBlend(m_pBits[py * m_screenWidth + px], c);
        }
    }

    // 绘制每个槽位的图标
    for (int slot = 0; slot < HOTBAR_SLOTS; ++slot)
    {
        int bt = types[slot];
        if (bt <= 0 || bt >= MAX_BLOCK_TYPE) continue;
        if (m_hotbarIcons[bt].empty()) continue;

        int nativeX = HB_SLOT_ORIGIN_X + slot * HB_SLOT_STEP;
        int nativeY = HB_SLOT_ORIGIN_Y;
        int screenX = hbX + (int) (nativeX * scale);
        int screenY = hbY + (int) (nativeY * scale);
        int sz = m_hbIconDisplaySize;

        for (int dy = 0; dy < sz; ++dy)
        {
            int py = screenY + dy;
            if (py < 0 || py >= m_screenHeight) continue;
            int dstRow = py * m_screenWidth;
            int srcRow = dy * sz;
            for (int dx = 0; dx < sz; ++dx)
            {
                COLORREF c = m_hotbarIcons[bt][srcRow + dx];
                if (c == 0) continue;
                int px = screenX + dx;
                if (px >= 0 && px < m_screenWidth)
                    m_pBits[dstRow + px] = alphaBlend(m_pBits[dstRow + px], c);
            }
        }

        // 数量文字（右下角）
        if (hotbarCounts && hotbarCounts[slot] > 1)
        {
            wchar_t buf[8];
            swprintf(buf, 8, L"%d", hotbarCounts[slot]);
            HFONT smallFont = CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Minecraft AE");
            HFONT oldF = (HFONT) SelectObject(m_memDC, smallFont);
            SetBkMode(m_memDC, TRANSPARENT);
            SetTextColor(m_memDC, RGB(255, 255, 255));
            SIZE ts;
            GetTextExtentPoint32W(m_memDC, buf, (int) wcslen(buf), &ts);
            TextOutW(m_memDC, screenX + sz - ts.cx - 1, screenY + sz - ts.cy + 1, buf, (int) wcslen(buf));
            SelectObject(m_memDC, oldF);
            DeleteObject(smallFont);
        }
    }

    // 绘制选中框（select.png 的 (3,3) 与选中槽位左上角对齐）
    if (!m_selectPixels.empty() && selectedSlot >= 0 && selectedSlot < HOTBAR_SLOTS)
    {
        // 选中槽位原生左上角 = (3 + slot*20, 3)
        // select 的 (3,3) 对上去 → select 绘制起点 = (slot*20*scale, 0) 相对 hbX,hbY
        int selX = hbX + (int) (selectedSlot * HB_SLOT_STEP * scale);
        int selY = hbY;
        for (int dy = 0; dy < m_selectH; ++dy)
        {
            int py = selY + dy;
            if (py < 0 || py >= m_screenHeight) continue;
            int dstRow = py * m_screenWidth;
            int srcRow = dy * m_selectW;
            for (int dx = 0; dx < m_selectW; ++dx)
            {
                COLORREF c = m_selectPixels[srcRow + dx];
                if (c == 0) continue;  // 全透明跳过
                int px = selX + dx;
                if (px >= 0 && px < m_screenWidth)
                    m_pBits[dstRow + px] = alphaBlend(m_pBits[dstRow + px], c);
            }
        }
    }

    // 刷新到屏幕
    HDC hdc = GetImageHDC();
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, m_memDC, 0, 0, SRCCOPY);
}


// ============================================================================
// 准星
// ============================================================================

void Renderer::drawCrosshair()
{
    int cx = m_screenWidth / 2;
    int cy = m_screenHeight / 2;
    int len = 8;

    setlinecolor(RGB(255, 255, 255));
    line(cx - len, cy, cx + len, cy);
    line(cx, cy - len, cx, cy + len);
}

// ============================================================================
// HUD
// ============================================================================

void Renderer::drawHUD(const Camera4D &cam)
{
    const Vec4 &pos = cam.getPos();
    const Vec4 &r = cam.getRight();
    const Vec4 &f = cam.getForward();
    const Vec4 &o = cam.getOver();

    wchar_t buf[256];
    HDC hdc = GetImageHDC();
    HFONT oldHudFont = m_hFont ? (HFONT) SelectObject(hdc, m_hFont) : nullptr;
    SetBkMode(hdc, TRANSPARENT);

    // ========================================
    // 左上角：XZW 三维坐标系可视化
    // ========================================
    const int vpX = 10, vpY = 10, vpW = 150, vpH = 140;
    const int ox = vpX + vpW / 2, oy = vpY + vpH / 2 + 5;
    const double vs = 45.0;

    // 黑底
    setfillcolor(BLACK);
    solidrectangle(vpX, vpY, vpX + vpW, vpY + vpH);

    auto proj3 = [&](double vx, double vz, double vw) -> POINT
    {
        const double k = 0.35355339;
        return {
            static_cast<int>(ox + vz * vs - vx * vs * k),
            static_cast<int>(oy - vw * vs + vx * vs * k)
        };
    };

    POINT oPt = proj3(0, 0, 0);

    // ---- n 的垂直平面（半透明粉色四边形） ----
    Vec4 n3 = Vec4(o.x, 0.0, o.z, o.w);
    double nLen = vec4Length(n3);
    if (nLen > 1e-9) { n3 = vec4Scale(n3, 1.0 / nLen); }

    Vec4 u3, v3;
    if (std::abs(n3.x) < 0.9)      u3 = Vec4(1.0, 0.0, 0.0, 0.0);
    else if (std::abs(n3.z) < 0.9) u3 = Vec4(0.0, 0.0, 1.0, 0.0);
    else                           u3 = Vec4(0.0, 0.0, 0.0, 1.0);
    double dotUN = u3.x * n3.x + u3.z * n3.z + u3.w * n3.w;
    u3 = Vec4(u3.x - dotUN * n3.x, 0.0, u3.z - dotUN * n3.z, u3.w - dotUN * n3.w);
    double uLen = std::sqrt(u3.x * u3.x + u3.z * u3.z + u3.w * u3.w);
    if (uLen > 1e-9) { u3.x /= uLen; u3.z /= uLen; u3.w /= uLen; }
    v3.x = n3.z * u3.w - n3.w * u3.z;
    v3.z = n3.w * u3.x - n3.x * u3.w;
    v3.w = n3.x * u3.z - n3.z * u3.x;

    const double pHalf = 1.1;
    POINT pCorners[4] = {
        proj3(u3.x * pHalf + v3.x * pHalf,  u3.z * pHalf + v3.z * pHalf,  u3.w * pHalf + v3.w * pHalf),
        proj3(u3.x * pHalf - v3.x * pHalf,  u3.z * pHalf - v3.z * pHalf,  u3.w * pHalf - v3.w * pHalf),
        proj3(-u3.x * pHalf - v3.x * pHalf, -u3.z * pHalf - v3.z * pHalf, -u3.w * pHalf - v3.w * pHalf),
        proj3(-u3.x * pHalf + v3.x * pHalf, -u3.z * pHalf + v3.z * pHalf, -u3.w * pHalf + v3.w * pHalf)
    };
    setfillcolor(RGB(80, 60, 90));
    setlinecolor(RGB(150, 100, 180));
    fillpolygon(pCorners, 4);

    // ---- 坐标轴 ----
    auto drawArrow2 = [](POINT from, POINT to, COLORREF clr)
    {
        setlinecolor(clr);
        line(from.x, from.y, to.x, to.y);
    };

    POINT xPt = proj3(1.2, 0, 0);
    POINT zPt = proj3(0, 1.2, 0);
    POINT wPt = proj3(0, 0, 1.2);
    drawArrow2(oPt, xPt, RGB(255, 100, 100));
    drawArrow2(oPt, zPt, RGB(100, 255, 100));
    drawArrow2(oPt, wPt, RGB(100, 150, 255));

    settextcolor(RGB(255, 100, 100)); TextOutW(hdc, xPt.x - 20, xPt.y + 2, L"X", 1);
    settextcolor(RGB(100, 255, 100)); TextOutW(hdc, zPt.x + 2, zPt.y - 8, L"Z", 1);
    settextcolor(RGB(100, 150, 255)); TextOutW(hdc, wPt.x - 8, wPt.y - 18, L"W", 1);

    // i（forward，黄色）
    Vec4 i3 = Vec4(f.x, 0.0, f.z, f.w);
    double iLen = vec4Length(i3);
    if (iLen > 1e-9) { i3 = vec4Scale(i3, 1.0 / iLen); }
    POINT iP = proj3(i3.x, i3.z, i3.w);
    drawArrow2(oPt, iP, RGB(255, 220, 50));
    settextcolor(RGB(255, 220, 50)); TextOutW(hdc, iP.x + 3, iP.y - 10, L"i", 1);

    // j（right，青色）
    Vec4 j3 = Vec4(r.x, 0.0, r.z, r.w);
    double jLen = vec4Length(j3);
    if (jLen > 1e-9) { j3 = vec4Scale(j3, 1.0 / jLen); }
    POINT jP = proj3(j3.x, j3.z, j3.w);
    drawArrow2(oPt, jP, RGB(50, 220, 220));
    settextcolor(RGB(50, 220, 220)); TextOutW(hdc, jP.x + 3, jP.y - 10, L"j", 1);

    // n（over，粉色法向）
    n3 = Vec4(o.x, 0.0, o.z, o.w);
    nLen = vec4Length(n3);
    if (nLen > 1e-9) { n3 = vec4Scale(n3, 1.0 / nLen); }
    POINT nP = proj3(n3.x, n3.z, n3.w);
    drawArrow2(oPt, nP, RGB(255, 120, 255));
    settextcolor(RGB(255, 120, 255)); TextOutW(hdc, nP.x + 3, nP.y - 10, L"n", 1);

    // ========================================
    // FPS + 诊断（右上角，F3 切换）
    // ========================================
    if (m_showHUD)
    {
        settextcolor(RGB(255, 255, 255));
        swprintf(buf, 256, L"FPS: %d", m_fps);
        TextOutW(hdc, m_screenWidth - 100, 10, buf, (int) wcslen(buf));

        settextcolor(RGB(255, 255, 100));
        swprintf(buf, 256, L"方块总数: %d", m_diagTotal);
        TextOutW(hdc, m_screenWidth - 200, 30, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"切片通过: %d", m_diagSlice);
        TextOutW(hdc, m_screenWidth - 200, 48, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"遮挡通过: %d", m_diagOccl);
        TextOutW(hdc, m_screenWidth - 200, 66, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"几何生成: %d", m_diagGeom);
        TextOutW(hdc, m_screenWidth - 200, 84, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"渲染面数: %d", m_diagFaces);
        TextOutW(hdc, m_screenWidth - 200, 102, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"面剔除: %d", m_diagFaceCull.load());
        TextOutW(hdc, m_screenWidth - 200, 120, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"区块: %d/%d", m_diagChunkPass, m_diagChunkTotal);
        TextOutW(hdc, m_screenWidth - 200, 138, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"线程: tri=%d tile=%d", m_diagThreads, m_diagTiles);
        TextOutW(hdc, m_screenWidth - 200, 156, buf, (int) wcslen(buf));

        settextcolor(RGB(180, 220, 255));
        swprintf(buf, 256, L"收集: %.1fms", m_msCollect);
        TextOutW(hdc, m_screenWidth - 200, 174, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"方块->三角形: %.1fms", m_msBlock2Tri);
        TextOutW(hdc, m_screenWidth - 200, 192, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"光栅化: %.1fms", m_msRaster);
        TextOutW(hdc, m_screenWidth - 200, 210, buf, (int) wcslen(buf));
        settextcolor(RGB(255, 255, 255));
    } // m_showHUD

    // ========================================
    // 坐标信息（左侧，坐标系下方）
    // ========================================
    int infoY = vpY + vpH + 5;
    swprintf(buf, 256, L"%.1f,  %.1f,  %.1f,  %.1f",
        pos.x, pos.y, pos.z, pos.w);
    TextOutW(hdc, 10, infoY, buf, (int) wcslen(buf));
    infoY += 18;

    const double rad2deg = 180.0 / 3.1415926535;
    swprintf(buf, 256, L"俯仰角: %+.1f", cam.getPitch() * rad2deg);
    TextOutW(hdc, 10, infoY, buf, (int) wcslen(buf));

    if (oldHudFont)
        SelectObject(hdc, oldHudFont);
}


// ============================================================================
// GUI：背景捕获与高斯模糊
// ============================================================================

void Renderer::captureBackground()
{
    if (!m_dibReady) return;

    int total = m_screenWidth * m_screenHeight;
    m_background.resize(total);
    for (int i = 0; i < total; ++i)
        m_background[i] = m_pBits[i];
    m_backgroundReady = true;
}

void Renderer::applyGaussianBlur()
{
    if (!m_backgroundReady) return;

    int w = m_screenWidth, h = m_screenHeight;
    std::vector<DWORD> temp(w * h);

    // 7-tap binomial kernel: [1, 6, 15, 20, 15, 6, 1] / 64
    // 4 次完整迭代 → 等效于 ~25-tap 核，sigma ≈ 3.5，强模糊
    constexpr int K = 7;
    constexpr int weights[K] = { 1, 6, 15, 20, 15, 6, 1 };
    constexpr int radius = K / 2;
    constexpr int ITERATIONS = 4;

    for (int iter = 0; iter < ITERATIONS; ++iter)
    {
        // ---- 水平模糊 ----
        for (int y = 0; y < h; ++y)
        {
            int rowBase = y * w;
            for (int x = 0; x < w; ++x)
            {
                int sumR = 0, sumG = 0, sumB = 0;
                int weightSum = 0;
                for (int k = -radius; k <= radius; ++k)
                {
                    int sx = x + k;
                    if (sx < 0) sx = 0;
                    if (sx >= w) sx = w - 1;
                    DWORD c = m_background[rowBase + sx];
                    int wt = weights[k + radius];
                    sumR += GetRValue(c) * wt;
                    sumG += GetGValue(c) * wt;
                    sumB += GetBValue(c) * wt;
                    weightSum += wt;
                }
                sumR /= weightSum;
                sumG /= weightSum;
                sumB /= weightSum;
                temp[rowBase + x] = RGB(sumR, sumG, sumB);
            }
        }

        // ---- 垂直模糊 ----
        for (int y = 0; y < h; ++y)
        {
            int rowBase = y * w;
            for (int x = 0; x < w; ++x)
            {
                int sumR = 0, sumG = 0, sumB = 0;
                int weightSum = 0;
                for (int k = -radius; k <= radius; ++k)
                {
                    int sy = y + k;
                    if (sy < 0) sy = 0;
                    if (sy >= h) sy = h - 1;
                    DWORD c = temp[sy * w + x];
                    int wt = weights[k + radius];
                    sumR += GetRValue(c) * wt;
                    sumG += GetGValue(c) * wt;
                    sumB += GetBValue(c) * wt;
                    weightSum += wt;
                }
                sumR /= weightSum;
                sumG /= weightSum;
                sumB /= weightSum;
                m_background[rowBase + x] = RGB(sumR, sumG, sumB);
            }
        }
    }
}

void Renderer::drawBackground()
{
    if (!m_backgroundReady || !m_dibReady) return;
    int total = m_screenWidth * m_screenHeight;
    for (int i = 0; i < total; ++i)
        m_pBits[i] = m_background[i];
}

void Renderer::flushToScreen()
{
    if (!m_dibReady) return;
    HDC hdc = GetImageHDC();
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, m_memDC, 0, 0, SRCCOPY);
}

// ============================================================================
// GUI：图片绘制
// ============================================================================

void Renderer::drawImageCentered(IMAGE *img)
{
    if (!img || !m_dibReady) return;

    DWORD *buf = GetImageBuffer(img);
    int iw = img->getwidth();
    int ih = img->getheight();
    if (!buf || iw <= 0 || ih <= 0) return;

    int ox = (m_screenWidth - iw) / 2;
    int oy = (m_screenHeight - ih) / 2;

    for (int y = 0; y < ih; ++y)
    {
        int py = oy + y;
        if (py < 0 || py >= m_screenHeight) continue;
        int srcRow = y * iw;
        int dstRow = py * m_screenWidth;
        for (int x = 0; x < iw; ++x)
        {
            int px = ox + x;
            if (px < 0 || px >= m_screenWidth) continue;
            DWORD c = buf[srcRow + x];
            // 跳过全透明（黑色背景也算——如果图片有 alpha，EasyX 会预乘到黑色）
            // 这里简单跳过纯黑像素，适用于 Minecraft 风格 GUI
            if (c == 0 || c == RGB(0, 0, 0)) continue;
            m_pBits[dstRow + px] = alphaBlend(m_pBits[dstRow + px], c);
        }
    }
}

// ============================================================================
// GUI：按钮绘制
// ============================================================================

void Renderer::drawButton(int x, int y, int w, int h,
    IMAGE *imgNormal, IMAGE *imgHover, IMAGE *imgActive,
    const wchar_t *text, bool hovered, bool pressed)
{
    if (!m_dibReady) return;

    IMAGE *useImg = imgNormal;
    if (pressed && imgActive) useImg = imgActive;
    else if (hovered && imgHover) useImg = imgHover;

    if (useImg)
    {
        // 贴图已用 loadimage 预缩放到按钮大小，1:1 复制
        DWORD *buf = GetImageBuffer(useImg);
        int iw = useImg->getwidth();
        int ih = useImg->getheight();
        if (buf && iw > 0 && ih > 0)
        {
            int copyW = iw < w ? iw : w;
            int copyH = ih < h ? ih : h;
            for (int dy = 0; dy < copyH; ++dy)
            {
                int py = y + dy;
                if (py < 0 || py >= m_screenHeight) continue;
                int dstRow = py * m_screenWidth;
                int srcRow = dy * iw;
                for (int dx = 0; dx < copyW; ++dx)
                {
                    int px = x + dx;
                    if (px < 0 || px >= m_screenWidth) continue;
                    DWORD c = buf[srcRow + dx];
                    if (c == 0 || c == RGB(0, 0, 0)) continue;
                    m_pBits[dstRow + px] = alphaBlend(m_pBits[dstRow + px], c);
                }
            }
        }

        // 2px 外边框：普通→黑色，悬停/按下→白色
        constexpr int BORDER_W = 2;
        const COLORREF borderColor = (hovered || pressed) ? RGB(255, 255, 255) : RGB(0, 0, 0);
        // 上边界（图片上方）
        for (int dy = 0; dy < BORDER_W; ++dy)
        {
            int py = y - BORDER_W + dy;
            if (py < 0 || py >= m_screenHeight) continue;
            int dstRow = py * m_screenWidth;
            for (int dx = -BORDER_W; dx < w + BORDER_W; ++dx)
            {
                int px = x + dx;
                if (px >= 0 && px < m_screenWidth)
                    m_pBits[dstRow + px] = borderColor;
            }
        }
        // 下边界（图片下方）
        for (int dy = 0; dy < BORDER_W; ++dy)
        {
            int py = y + h + dy;
            if (py < 0 || py >= m_screenHeight) continue;
            int dstRow = py * m_screenWidth;
            for (int dx = -BORDER_W; dx < w + BORDER_W; ++dx)
            {
                int px = x + dx;
                if (px >= 0 && px < m_screenWidth)
                    m_pBits[dstRow + px] = borderColor;
            }
        }
        // 左右边界（仅中间段，上下已由上面覆盖）
        for (int dy = 0; dy < h; ++dy)
        {
            int py = y + dy;
            if (py < 0 || py >= m_screenHeight) continue;
            int dstRow = py * m_screenWidth;
            // 左边界
            for (int bx = 0; bx < BORDER_W; ++bx)
            {
                int px = x - BORDER_W + bx;
                if (px >= 0 && px < m_screenWidth)
                    m_pBits[dstRow + px] = borderColor;
            }
            // 右边界
            for (int bx = 0; bx < BORDER_W; ++bx)
            {
                int px = x + w + bx;
                if (px >= 0 && px < m_screenWidth)
                    m_pBits[dstRow + px] = borderColor;
            }
        }
    }
    else
    {
        // 无贴图时绘制纯色按钮
        COLORREF bg = pressed ? RGB(80, 80, 80) : (hovered ? RGB(140, 140, 140) : RGB(100, 100, 100));
        for (int dy = 0; dy < h; ++dy)
        {
            int py = y + dy;
            if (py < 0 || py >= m_screenHeight) continue;
            int dstRow = py * m_screenWidth;
            for (int dx = 0; dx < w; ++dx)
            {
                int px = x + dx;
                if (px < 0 || px >= m_screenWidth) continue;
                m_pBits[dstRow + px] = bg;
            }
        }
        // 边框
        COLORREF border = hovered ? RGB(255, 255, 255) : RGB(180, 180, 180);
        for (int dx = 0; dx < w; ++dx)
        {
            if (y >= 0 && y < m_screenHeight) m_pBits[y * m_screenWidth + (x + dx)] = border;
            int by = y + h - 1;
            if (by >= 0 && by < m_screenHeight) m_pBits[by * m_screenWidth + (x + dx)] = border;
        }
        for (int dy = 0; dy < h; ++dy)
        {
            if (y + dy >= 0 && y + dy < m_screenHeight) m_pBits[(y + dy) * m_screenWidth + x] = border;
            int bx = x + w - 1;
            if (bx >= 0 && bx < m_screenWidth) m_pBits[(y + dy) * m_screenWidth + bx] = border;
        }
    }

    // 绘制文字（使用大号 Minecraft AE 字体）
    if (text && text[0])
    {
        int tx = x + w / 2;
        int ty = y + h / 2;
        HFONT oldFont = m_hFontLarge ? (HFONT) SelectObject(m_memDC, m_hFontLarge) : nullptr;
        SetBkMode(m_memDC, TRANSPARENT);
        SetTextColor(m_memDC, RGB(255, 255, 255));
        SIZE textSize;
        GetTextExtentPoint32W(m_memDC, text, (int) wcslen(text), &textSize);
        TextOutW(m_memDC, tx - textSize.cx / 2, ty - textSize.cy / 2, text, (int) wcslen(text));
        if (oldFont)
            SelectObject(m_memDC, oldFont);
    }
}
