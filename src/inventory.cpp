#include "inventory.h"
#include "crafting.h"

Inventory::Inventory()
{
    m_slots[0] = { BLOCK_GRASS,  64 };
    m_slots[1] = { BLOCK_DIRT,   64 };
    m_slots[2] = { BLOCK_LOG,    64 };
    m_slots[3] = { BLOCK_LEAVES, 64 };
    m_slots[4] = { BLOCK_STONE,  64 };
    m_slots[5] = { BLOCK_PLANKS, 64 };
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
        nx = CRAFT_X + (ci % 2) * SLOT_STEP;
        ny = (ci / 2) ? CRAFT_Y1 : CRAFT_Y0;
    }
    else
    {
        nx = OUTPUT_X;
        ny = OUTPUT_Y;
    }

    outX = imgDispX + (int) (nx * sx);
    outY = imgDispY + (int) (ny * sy);
    outW = (int) (SLOT_SIZE * sx);
    outH = (int) (SLOT_SIZE * sy);
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

    // 输出区（优先命中，因为面积大）
    if (nx >= OUTPUT_X && nx < OUTPUT_X + SLOT_SIZE &&
        ny >= OUTPUT_Y && ny < OUTPUT_Y + SLOT_SIZE)
        return CRAFT_OUTPUT_IDX;

    // 合成区 2×2
    for (int ci = 0; ci < CRAFT_INPUT; ++ci)
    {
        int cx = CRAFT_X + (ci % 2) * SLOT_STEP;
        int cy = (ci / 2) ? CRAFT_Y1 : CRAFT_Y0;
        if (nx >= cx && nx < cx + SLOT_SIZE && ny >= cy && ny < cy + SLOT_SIZE)
            return CRAFT_BASE + ci;
    }

    // 快捷栏
    for (int i = 0; i < HOTBAR_SLOTS; ++i)
    {
        int hx = BP_ORIGIN_X + i * SLOT_STEP;
        if (nx >= hx && nx < hx + SLOT_SIZE && ny >= HB_INV_Y && ny < HB_INV_Y + SLOT_SIZE)
            return i;
    }

    // 背包
    for (int r = 0; r < BACKPACK_ROWS; ++r)
        for (int c = 0; c < BACKPACK_COLS; ++c)
        {
            int bx = BP_ORIGIN_X + c * SLOT_STEP;
            int by = BP_ORIGIN_Y + r * SLOT_STEP;
            if (nx >= bx && nx < bx + SLOT_SIZE && ny >= by && ny < by + SLOT_SIZE)
                return HOTBAR_SLOTS + r * BACKPACK_COLS + c;
        }

    return -1;
}

void Inventory::updateCraftingResult(const CraftingManager &craftMgr)
{
    // 将 2×2 合成区填入 3×3 左上角
    int grid[3][3] = {};
    for (int ci = 0; ci < CRAFT_INPUT; ++ci)
    {
        int row = ci / 2, col = ci % 2;
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

bool Inventory::pickup(int slotIndex, int count /* = -1 */)
{
    if (slotIndex < 0 || slotIndex >= TOTAL_SLOTS) return false;
    if (isDragging()) return false;  // 手上已有物品，请用 addToDrag 或 placeInto
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

    // 不能放入输出区
    if (slotIndex == CRAFT_OUTPUT_IDX) return false;

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
    // 交换
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

bool Inventory::takeCraftOutput(const CraftingManager &craftMgr)
{
    auto &out = m_slots[CRAFT_OUTPUT_IDX];
    if (out.blockType == BLOCK_AIR || out.count <= 0) return false;

    if (isDragging())
    {
        // 手上已有物品 → 必须同种才能累加
        if (m_dragType != out.blockType) return false;
        m_dragCount += out.count;
    }
    else
    {
        // 拿起产物
        m_dragging = CRAFT_OUTPUT_IDX;
        m_dragType = out.blockType;
        m_dragCount = out.count;
    }
    out.blockType = BLOCK_AIR;
    out.count = 0;

    // 消耗合成区材料（每个非空格减 1）
    for (int ci = 0; ci < CRAFT_INPUT; ++ci)
    {
        auto &s = m_slots[CRAFT_BASE + ci];
        if (s.blockType != BLOCK_AIR && s.count > 0)
        {
            --s.count;
            if (s.count <= 0)
                s.blockType = BLOCK_AIR;
        }
    }

    // 重新计算合成结果
    updateCraftingResult(craftMgr);
    return true;
}
