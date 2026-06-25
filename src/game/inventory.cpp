#include "inventory.h"
#include "crafting.h"

Inventory::Inventory()
{

    m_slots[0] = { BLOCK_GRASS,  64 };
    m_slots[1] = { BLOCK_DIRT,   64 };
    m_slots[2] = { BLOCK_STONE,  64 };
    m_slots[3] = { BLOCK_LOG,    64 };
    m_slots[4] = { BLOCK_PLANKS, 64 };
    m_slots[5] = { BLOCK_LEAVES, 64 };
    m_slots[6] = { BLOCK_CRAFTING_TABLE, 1 };
    m_slots[7] = { BLOCK_STICK,  64 };

    m_slots[9] = { BLOCK_DIAMOND_ORE, 64 };
    m_slots[10] = { BLOCK_GOLD_ORE,    64 };
    m_slots[11] = { BLOCK_IRON_ORE,    64 };
    m_slots[12] = { BLOCK_DIAMOND,     64 };
    m_slots[13] = { BLOCK_GOLD_INGOT,  64 };
    m_slots[14] = { BLOCK_IRON_INGOT,  64 };
    m_slots[15] = { BLOCK_GOLD_NUGGET, 64 };
    m_slots[16] = { BLOCK_IRON_NUGGET, 64 };
    m_slots[17] = { BLOCK_APPLE,       64 };

    m_slots[18] = { BLOCK_DIAMOND_BLOCK, 64 };
    m_slots[19] = { BLOCK_GOLD_BLOCK,    64 };
    m_slots[20] = { BLOCK_IRON_BLOCK,    64 };
    m_slots[21] = { BLOCK_DIAMOND_PICKAXE, 1 };
    m_slots[22] = { BLOCK_DIAMOND_AXE,     1 };
    m_slots[23] = { BLOCK_DIAMOND_SWORD,   1 };
    m_slots[24] = { BLOCK_DIAMOND_SHOVEL,  1 };
    m_slots[25] = { BLOCK_DIAMOND_HOE,     1 };
    m_slots[26] = { BLOCK_GOLDEN_APPLE,   64 };

    m_slots[27] = { BLOCK_DIAMOND_HELMET,      1 };
    m_slots[28] = { BLOCK_DIAMOND_CHESTPLATE,  1 };
    m_slots[29] = { BLOCK_DIAMOND_LEGGINGS,    1 };
    m_slots[30] = { BLOCK_DIAMOND_BOOTS,       1 };
}

Inventory::Slot &Inventory::getSlot(int index)
{
    static Slot dummy;
    if (index < 0 || index >= TOTAL_SLOTS) return dummy;
    return m_slots[index];
}

const Inventory::Slot &Inventory::getSlot(int index) const
{
    static Slot dummy;
    if (index < 0 || index >= TOTAL_SLOTS) return dummy;
    return m_slots[index];
}

int Inventory::hotbarBlockType(int hotbarIndex) const
{
    if (hotbarIndex < 0 || hotbarIndex >= HOTBAR_SLOTS) return BLOCK_AIR;
    return m_slots[hotbarIndex].blockType;
}

void Inventory::setHotbar(int hotbarIndex, int blockType, int count)
{
    if (hotbarIndex < 0 || hotbarIndex >= HOTBAR_SLOTS) return;
    m_slots[hotbarIndex].blockType = blockType;
    m_slots[hotbarIndex].count = count;
}

void Inventory::slotScreenRect(int index,
    int imgDispX, int imgDispY, int imgDispW, int imgDispH,
    int imgNativeW, int imgNativeH,
    int &outX, int &outY, int &outW, int &outH) const
{
    outX = outY = outW = outH = 0;
    if (imgNativeW <= 0 || imgNativeH <= 0) return;
    double sx = (double) imgDispW / imgNativeW;
    double sy = (double) imgDispH / imgNativeH;

    bool isCT = (m_craftMode == CM_CraftingTable3x3);
    int craftW = isCT ? 3 : 2;
    int craftX = isCT ? CT_CRAFT_X : INV_CRAFT_X;
    int craftY = isCT ? CT_CRAFT_Y : INV_CRAFT_Y0;
    int outX0 = isCT ? CT_OUTPUT_X : INV_OUTPUT_X;
    int outY0 = isCT ? CT_OUTPUT_Y : INV_OUTPUT_Y;
    int outSz = isCT ? CT_OUTPUT_SIZE : SLOT_SIZE;

    int nx = 0, ny = 0;
    if (index < HOTBAR_SLOTS)
    {
        nx = BP_ORIGIN_X + index * SLOT_STEP;
        ny = HB_INV_Y;
    }
    else if (index < CRAFT_BASE)
    {
        int bp = index - HOTBAR_SLOTS;
        nx = BP_ORIGIN_X + (bp % BACKPACK_COLS) * SLOT_STEP;
        ny = BP_ORIGIN_Y + (bp / BACKPACK_COLS) * SLOT_STEP;
    }
    else if (index < CRAFT_OUTPUT_IDX)
    {
        int ci = index - CRAFT_BASE;
        nx = craftX + (ci % craftW) * SLOT_STEP;
        ny = craftY + (ci / craftW) * SLOT_STEP;
    }
    else if (index < ARMOR_BASE)
    {
        nx = outX0;
        ny = outY0;
    }
    else
    {

        int ai = index - ARMOR_BASE;
        nx = ARMOR_X;
        ny = armorSlotNativeY(ai);
    }

    outX = imgDispX + (int) (nx * sx);
    outY = imgDispY + (int) (ny * sy);
    int slotSz = (index == CRAFT_OUTPUT_IDX) ? outSz : SLOT_SIZE;
    outW = (int) (slotSz * sx);
    outH = (int) (slotSz * sy);
}

int Inventory::hitTest(int screenX, int screenY,
    int imgDispX, int imgDispY, int imgDispW, int imgDispH,
    int imgNativeW, int imgNativeH) const
{
    if (imgNativeW <= 0 || imgNativeH <= 0) return -1;
    double sx = (double) imgDispW / imgNativeW;
    double sy = (double) imgDispH / imgNativeH;
    double nx = (screenX - imgDispX) / sx;
    double ny = (screenY - imgDispY) / sy;

    bool isCT = (m_craftMode == CM_CraftingTable3x3);
    int craftW = isCT ? 3 : 2;
    int craftH = isCT ? 3 : 2;
    int craftX = isCT ? CT_CRAFT_X : INV_CRAFT_X;
    int craftY = isCT ? CT_CRAFT_Y : INV_CRAFT_Y0;
    int outX0 = isCT ? CT_OUTPUT_X : INV_OUTPUT_X;
    int outY0 = isCT ? CT_OUTPUT_Y : INV_OUTPUT_Y;
    int outSz = isCT ? CT_OUTPUT_SIZE : SLOT_SIZE;

    if (nx >= outX0 && nx < outX0 + outSz &&
        ny >= outY0 && ny < outY0 + outSz)
        return CRAFT_OUTPUT_IDX;

    for (int ci = 0; ci < craftW * craftH; ++ci)
    {
        int cx = craftX + (ci % craftW) * SLOT_STEP;
        int cy = craftY + (ci / craftW) * SLOT_STEP;
        if (nx >= cx && nx < cx + SLOT_SIZE && ny >= cy && ny < cy + SLOT_SIZE)
            return CRAFT_BASE + ci;
    }

    for (int i = 0; i < HOTBAR_SLOTS; ++i)
    {
        int hx = BP_ORIGIN_X + i * SLOT_STEP;
        if (nx >= hx && nx < hx + SLOT_SIZE && ny >= HB_INV_Y && ny < HB_INV_Y + SLOT_SIZE)
            return i;
    }

    for (int r = 0; r < BACKPACK_ROWS; ++r)
        for (int c = 0; c < BACKPACK_COLS; ++c)
        {
            int bx = BP_ORIGIN_X + c * SLOT_STEP;
            int by = BP_ORIGIN_Y + r * SLOT_STEP;
            if (nx >= bx && nx < bx + SLOT_SIZE && ny >= by && ny < by + SLOT_SIZE)
                return HOTBAR_SLOTS + r * BACKPACK_COLS + c;
        }

    for (int ai = 0; ai < ARMOR_SLOTS; ++ai)
    {
        int ay = armorSlotNativeY(ai);
        if (nx >= ARMOR_X && nx < ARMOR_X + SLOT_SIZE && ny >= ay && ny < ay + SLOT_SIZE)
            return ARMOR_BASE + ai;
    }

    return -1;
}

void Inventory::updateCraftingResult(const CraftingManager &craftMgr)
{
    int craftW = (m_craftMode == CM_CraftingTable3x3) ? 3 : 2;
    int craftH = (m_craftMode == CM_CraftingTable3x3) ? 3 : 2;

    int grid[3][3] = {};
    for (int ci = 0; ci < craftW * craftH; ++ci)
    {
        int row = ci / craftW, col = ci % craftW;
        grid[row][col] = m_slots[CRAFT_BASE + ci].blockType;
    }

    CraftResult r = craftMgr.match(grid);
    auto &out = m_slots[CRAFT_OUTPUT_IDX];
    if (r.valid)
    {
        out.blockType = r.outputType;
        out.count = r.outputCount;
    }
    else
    {
        out.blockType = BLOCK_AIR;
        out.count = 0;
    }
}

bool Inventory::pickup(int slotIndex, int count)
{
    if (slotIndex < 0 || slotIndex >= TOTAL_SLOTS) return false;
    if (isDragging()) return false;
    auto &s = m_slots[slotIndex];
    if (s.blockType == BLOCK_AIR || s.count <= 0) return false;

    int take = (count < 0 || count > s.count) ? s.count : count;
    if (take <= 0) return false;

    m_dragging = slotIndex;
    m_dragType = s.blockType;
    m_dragCount = take;

    s.count -= take;
    if (s.count <= 0)
    {
        s.blockType = BLOCK_AIR;
        s.count = 0;
    }
    return true;
}

bool Inventory::addToDrag(int slotIndex, int count)
{
    if (!isDragging()) return false;
    if (slotIndex < 0 || slotIndex >= TOTAL_SLOTS) return false;
    auto &s = m_slots[slotIndex];
    if (s.blockType != m_dragType || s.count <= 0) return false;

    int take = (count > s.count) ? s.count : count;
    if (take <= 0) return false;

    m_dragCount += take;
    s.count -= take;
    if (s.count <= 0)
    {
        s.blockType = BLOCK_AIR;
        s.count = 0;
    }
    return true;
}

bool Inventory::placeInto(int slotIndex)
{
    if (!isDragging()) return false;
    if (slotIndex < 0 || slotIndex >= TOTAL_SLOTS) { cancelDrag(); return false; }

    if (slotIndex == CRAFT_OUTPUT_IDX) return false;

    if (slotIndex >= ARMOR_BASE && slotIndex < ARMOR_BASE + ARMOR_SLOTS)
    {
        int ai = slotIndex - ARMOR_BASE;
        if (!isValidArmorForSlot(ai, m_dragType)) return false;
        if (m_dragCount != 1) return false;
    }

    auto &s = m_slots[slotIndex];
    if (s.blockType == BLOCK_AIR)
    {
        s.blockType = m_dragType;
        s.count = m_dragCount;
        m_dragging = -1;
        return true;
    }
    if (s.blockType == m_dragType)
    {
        s.count += m_dragCount;
        m_dragging = -1;
        return true;
    }

    int tt = s.blockType, tc = s.count;
    s.blockType = m_dragType;
    s.count = m_dragCount;
    m_dragType = tt;
    m_dragCount = tc;
    return true;
}

bool Inventory::placeOneInto(int slotIndex)
{
    if (!isDragging()) return false;
    if (slotIndex < 0 || slotIndex >= TOTAL_SLOTS) return false;
    if (slotIndex == CRAFT_OUTPUT_IDX) return false;

    if (slotIndex >= ARMOR_BASE && slotIndex < ARMOR_BASE + ARMOR_SLOTS) return false;

    auto &s = m_slots[slotIndex];
    if (s.blockType != BLOCK_AIR && s.blockType != m_dragType) return false;

    if (s.blockType == BLOCK_AIR)
    {
        s.blockType = m_dragType;
        s.count = 1;
    }
    else
    {
        s.count += 1;
    }

    m_dragCount -= 1;
    if (m_dragCount <= 0)
    {
        m_dragType = BLOCK_AIR;
        m_dragging = -1;
    }
    return true;
}

void Inventory::cancelDrag()
{
    if (!isDragging()) return;
    auto &s = m_slots[m_dragging];
    s.blockType = m_dragType;
    s.count = m_dragCount;
    m_dragging = -1;
}

int Inventory::armorSlotNativeY(int subIndex)
{
    switch (subIndex)
    {
        case 0: return ARMOR_Y0;
        case 1: return ARMOR_Y1;
        case 2: return ARMOR_Y2;
        case 3: return ARMOR_Y3;
        default: return 0;
    }
}

bool Inventory::isValidArmorForSlot(int armorSubIndex, int blockType)
{
    switch (armorSubIndex)
    {
        case 0:
            return blockType == BLOCK_IRON_HELMET || blockType == BLOCK_GOLDEN_HELMET ||
                blockType == BLOCK_DIAMOND_HELMET;
        case 1:
            return blockType == BLOCK_IRON_CHESTPLATE || blockType == BLOCK_GOLDEN_CHESTPLATE ||
                blockType == BLOCK_DIAMOND_CHESTPLATE;
        case 2:
            return blockType == BLOCK_IRON_LEGGINGS || blockType == BLOCK_GOLDEN_LEGGINGS ||
                blockType == BLOCK_DIAMOND_LEGGINGS;
        case 3:
            return blockType == BLOCK_IRON_BOOTS || blockType == BLOCK_GOLDEN_BOOTS ||
                blockType == BLOCK_DIAMOND_BOOTS;
        default: return false;
    }
}

bool Inventory::takeCraftOutput(const CraftingManager &craftMgr)
{
    auto &out = m_slots[CRAFT_OUTPUT_IDX];
    if (out.blockType == BLOCK_AIR || out.count <= 0) return false;

    if (isDragging())
    {

        if (m_dragType != out.blockType) return false;
        m_dragCount += out.count;
    }
    else
    {

        m_dragging = CRAFT_OUTPUT_IDX;
        m_dragType = out.blockType;
        m_dragCount = out.count;
    }
    out.blockType = BLOCK_AIR;
    out.count = 0;

    int craftW = (m_craftMode == CM_CraftingTable3x3) ? 3 : 2;
    int craftH = (m_craftMode == CM_CraftingTable3x3) ? 3 : 2;
    for (int ci = 0; ci < craftW * craftH; ++ci)
    {
        auto &s = m_slots[CRAFT_BASE + ci];
        if (s.blockType != BLOCK_AIR && s.count > 0)
        {
            --s.count;
            if (s.count <= 0)
                s.blockType = BLOCK_AIR;
        }
    }

    updateCraftingResult(craftMgr);
    return true;
}