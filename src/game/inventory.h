#pragma once

#include "../core/constant.h"

class CraftingManager;

class Inventory
{
public:
    static constexpr int HOTBAR_SLOTS = 9;
    static constexpr int BACKPACK_ROWS = 3;
    static constexpr int BACKPACK_COLS = 9;
    static constexpr int BACKPACK_SLOTS = BACKPACK_ROWS * BACKPACK_COLS;
    static constexpr int CRAFT_INPUT = 9;
    static constexpr int CRAFT_OUTPUT = 1;
    static constexpr int ARMOR_SLOTS = 4;
    static constexpr int FURNACE_SLOTS = 3;
    static constexpr int TOTAL_SLOTS = HOTBAR_SLOTS + BACKPACK_SLOTS + CRAFT_INPUT + CRAFT_OUTPUT + ARMOR_SLOTS + FURNACE_SLOTS;

    enum CraftMode { CM_Inventory2x2, CM_CraftingTable3x3 };

    static constexpr int SLOT_SIZE = 16;
    static constexpr int SLOT_STEP = 18;
    static constexpr int BP_ORIGIN_X = 8;
    static constexpr int BP_ORIGIN_Y = 84;
    static constexpr int HB_INV_Y = 142;

    static constexpr int INV_CRAFT_X = 98;
    static constexpr int INV_CRAFT_Y0 = 18;
    static constexpr int INV_CRAFT_Y1 = 36;
    static constexpr int INV_OUTPUT_X = 154;
    static constexpr int INV_OUTPUT_Y = 28;

    static constexpr int ARMOR_X = 8;
    static constexpr int ARMOR_Y0 = 8;
    static constexpr int ARMOR_Y1 = 26;
    static constexpr int ARMOR_Y2 = 44;
    static constexpr int ARMOR_Y3 = 62;
    static constexpr int ARMOR_GAP = 2;

    static constexpr int CT_CRAFT_X = 30;
    static constexpr int CT_CRAFT_Y = 17;
    static constexpr int CT_OUTPUT_X = 124;
    static constexpr int CT_OUTPUT_Y = 35;
    static constexpr int CT_OUTPUT_SIZE = 16;

    struct Slot
    {
        int blockType = BLOCK_AIR;
        int count = 0;
    };

    Inventory();

    Slot &getSlot(int index);
    const Slot &getSlot(int index) const;
    int hotbarBlockType(int hotbarIndex) const;
    void setHotbar(int hotbarIndex, int blockType, int count);

    void setCraftMode(CraftMode mode) { m_craftMode = mode; }
    CraftMode craftMode() const { return m_craftMode; }

    void updateCraftingResult(const CraftingManager &craftMgr);
    int  craftOutputType()  const { return m_slots[CRAFT_OUTPUT_IDX].blockType; }
    int  craftOutputCount() const { return m_slots[CRAFT_OUTPUT_IDX].count; }

    bool takeCraftOutput(const CraftingManager &craftMgr);

    void slotScreenRect(int index,
        int imgDispX, int imgDispY, int imgDispW, int imgDispH,
        int imgNativeW, int imgNativeH,
        int &outX, int &outY, int &outW, int &outH) const;
    int hitTest(int screenX, int screenY,
        int imgDispX, int imgDispY, int imgDispW, int imgDispH,
        int imgNativeW, int imgNativeH) const;

    bool pickup(int slotIndex, int count = -1);

    bool placeInto(int slotIndex);

    bool placeOneInto(int slotIndex);

    bool addToDrag(int slotIndex, int count);
    bool isDragging()  const { return m_dragging != -1; }
    int  dragBlockType() const { return m_dragType; }
    int  dragCount()    const { return m_dragCount; }
    void cancelDrag();

    void returnCraftItems();
    bool collectAll(int slotIndex);

    static constexpr int ARMOR_BASE = HOTBAR_SLOTS + BACKPACK_SLOTS + CRAFT_INPUT + CRAFT_OUTPUT;
    static bool isValidArmorForSlot(int armorSubIndex, int blockType);
    static int armorSlotNativeY(int subIndex);

private:
    static constexpr int CRAFT_BASE = HOTBAR_SLOTS + BACKPACK_SLOTS;
    static constexpr int CRAFT_OUTPUT_IDX = CRAFT_BASE + CRAFT_INPUT;

    Slot m_slots[TOTAL_SLOTS];

    CraftMode m_craftMode = CM_Inventory2x2;

    int m_dragging = -1;
    int m_dragType = BLOCK_AIR;
    int m_dragCount = 0;
};