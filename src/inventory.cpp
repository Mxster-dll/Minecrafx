#include "inventory.h"

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

    double scaleX = (double) imgDispW / imgNativeW;
    double scaleY = (double) imgDispH / imgNativeH;

    int nativeX, nativeY;
    if (index < HOTBAR_SLOTS)
    {
        nativeX = BP_ORIGIN_X + index * SLOT_STEP;
        nativeY = HB_INV_Y;
    }
    else
    {
        int bpIdx = index - HOTBAR_SLOTS;
        int row = bpIdx / BACKPACK_COLS;
        int col = bpIdx % BACKPACK_COLS;
        nativeX = BP_ORIGIN_X + col * SLOT_STEP;
        nativeY = BP_ORIGIN_Y + row * SLOT_STEP;
    }

    outX = imgDispX + (int) (nativeX * scaleX);
    outY = imgDispY + (int) (nativeY * scaleY);
    outW = (int) (SLOT_SIZE * scaleX);
    outH = (int) (SLOT_SIZE * scaleY);
}

int Inventory::hitTest(int screenX, int screenY,
    int imgDispX, int imgDispY, int imgDispW, int imgDispH,
    int imgNativeW, int imgNativeH) const
{
    if (imgNativeW <= 0 || imgNativeH <= 0) return -1;

    double scaleX = (double) imgDispW / imgNativeW;
    double scaleY = (double) imgDispH / imgNativeH;

    double nativeX = (screenX - imgDispX) / scaleX;
    double nativeY = (screenY - imgDispY) / scaleY;

    for (int i = 0; i < HOTBAR_SLOTS; ++i)
    {
        int nx = BP_ORIGIN_X + i * SLOT_STEP;
        int ny = HB_INV_Y;
        if (nativeX >= nx && nativeX < nx + SLOT_SIZE &&
            nativeY >= ny && nativeY < ny + SLOT_SIZE)
            return i;
    }
    for (int row = 0; row < BACKPACK_ROWS; ++row)
    {
        for (int col = 0; col < BACKPACK_COLS; ++col)
        {
            int nx = BP_ORIGIN_X + col * SLOT_STEP;
            int ny = BP_ORIGIN_Y + row * SLOT_STEP;
            if (nativeX >= nx && nativeX < nx + SLOT_SIZE &&
                nativeY >= ny && nativeY < ny + SLOT_SIZE)
                return HOTBAR_SLOTS + row * BACKPACK_COLS + col;
        }
    }
    return -1;
}

bool Inventory::pickup(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= TOTAL_SLOTS) return false;
    auto &s = m_slots[slotIndex];
    if (s.blockType == BLOCK_AIR || s.count <= 0) return false;

    m_dragging = slotIndex;
    m_dragType = s.blockType;
    m_dragCount = s.count;
    s.blockType = BLOCK_AIR;
    s.count = 0;
    return true;
}

bool Inventory::placeInto(int slotIndex)
{
    if (!isDragging()) return false;
    if (slotIndex < 0 || slotIndex >= TOTAL_SLOTS) { cancelDrag(); return false; }

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
    // 不同物品：交换
    int tmpType = s.blockType, tmpCount = s.count;
    s.blockType = m_dragType;
    s.count = m_dragCount;
    m_dragType = tmpType;
    m_dragCount = tmpCount;
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
