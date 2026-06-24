#pragma once

#include "constant.h"

class CraftingManager;

// ============================================================================
// Inventory — 背包 + 快捷栏 + 合成区（共 41 格），支持拖拽
// ============================================================================
class Inventory
{
public:
    static constexpr int HOTBAR_SLOTS = 9;
    static constexpr int BACKPACK_ROWS = 3;
    static constexpr int BACKPACK_COLS = 9;
    static constexpr int BACKPACK_SLOTS = BACKPACK_ROWS * BACKPACK_COLS;
    static constexpr int CRAFT_INPUT = 4;   // 2×2 合成台
    static constexpr int CRAFT_OUTPUT = 1;
    static constexpr int TOTAL_SLOTS = HOTBAR_SLOTS + BACKPACK_SLOTS + CRAFT_INPUT + CRAFT_OUTPUT;

    // 槽位在 inventory.png 原生坐标中的参数
    static constexpr int SLOT_SIZE = 16;
    static constexpr int SLOT_STEP = 18;
    static constexpr int BP_ORIGIN_X = 8;
    static constexpr int BP_ORIGIN_Y = 84;
    static constexpr int HB_INV_Y = 142;
    // 合成区
    static constexpr int CRAFT_X = 98;
    static constexpr int CRAFT_Y0 = 18;   // 第一排 y
    static constexpr int CRAFT_Y1 = 36;   // 第二排 y
    static constexpr int OUTPUT_X = 154;
    static constexpr int OUTPUT_Y = 28;

    struct Slot
    {
        int blockType = BLOCK_AIR;
        int count = 0;
    };

    Inventory();

    // ---- 访问 ----
    Slot &getSlot(int index);
    const Slot &getSlot(int index) const;
    int hotbarBlockType(int hotbarIndex) const;
    void setHotbar(int hotbarIndex, int blockType, int count);

    // ---- 合成 ----
    /** @brief 当前合成输出（需每次合成区变化后调用） */
    void updateCraftingResult(const CraftingManager &craftMgr);
    int  craftOutputType()  const { return m_slots[CRAFT_OUTPUT_IDX].blockType; }
    int  craftOutputCount() const { return m_slots[CRAFT_OUTPUT_IDX].count; }

    /** @brief 从输出格拿取产物（同时消耗合成区材料并刷新） */
    bool takeCraftOutput(const CraftingManager &craftMgr);

    // ---- 坐标换算 ----
    void slotScreenRect(int index,
        int imgDispX, int imgDispY, int imgDispW, int imgDispH,
        int imgNativeW, int imgNativeH,
        int &outX, int &outY, int &outW, int &outH) const;
    int hitTest(int screenX, int screenY,
        int imgDispX, int imgDispY, int imgDispW, int imgDispH,
        int imgNativeW, int imgNativeH) const;

    // ---- 拖拽 ----
    /** @brief 从槽位拿取物品。count=-1 拿全部，count>0 只拿指定数量 */
    bool pickup(int slotIndex, int count = -1);
    /** @brief 将手上物品放入槽位 */
    bool placeInto(int slotIndex);
    /** @brief 将手上 1 个物品放入槽位（右键用） */
    bool placeOneInto(int slotIndex);
    /** @brief 拖拽时从槽位取 count 个物品累加到手上（类型必须相同） */
    bool addToDrag(int slotIndex, int count);
    bool isDragging()  const { return m_dragging != -1; }
    int  dragBlockType() const { return m_dragType; }
    int  dragCount()    const { return m_dragCount; }
    void cancelDrag();

private:
    static constexpr int CRAFT_BASE = HOTBAR_SLOTS + BACKPACK_SLOTS;
    static constexpr int CRAFT_OUTPUT_IDX = CRAFT_BASE + CRAFT_INPUT;

    Slot m_slots[TOTAL_SLOTS];

    int m_dragging = -1;
    int m_dragType = BLOCK_AIR;
    int m_dragCount = 0;
};
