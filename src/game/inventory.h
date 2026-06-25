#pragma once

#include "../core/constant.h"

class CraftingManager;

// ============================================================================
// Inventory — 背包 + 快捷栏 + 合成区（热键栏 9 + 背包 27 + 合成输入 9 + 输出 1 = 46 格）
// 支持两种合成模式：背包 2×2 和 工作台 3×3
// ============================================================================
class Inventory
{
public:
    static constexpr int HOTBAR_SLOTS = 9;
    static constexpr int BACKPACK_ROWS = 3;
    static constexpr int BACKPACK_COLS = 9;
    static constexpr int BACKPACK_SLOTS = BACKPACK_ROWS * BACKPACK_COLS;
    static constexpr int CRAFT_INPUT = 9;   // 最多 3×3 合成台
    static constexpr int CRAFT_OUTPUT = 1;
    static constexpr int ARMOR_SLOTS = 4;   // 头盔/胸甲/护腿/靴子
    static constexpr int FURNACE_SLOTS = 3; // 熔炉输入/燃料/输出
    static constexpr int TOTAL_SLOTS = HOTBAR_SLOTS + BACKPACK_SLOTS + CRAFT_INPUT + CRAFT_OUTPUT + ARMOR_SLOTS + FURNACE_SLOTS;

    enum CraftMode { CM_Inventory2x2, CM_CraftingTable3x3 };

    // ---- inventory.png 的合成区坐标（2×2） ----
    static constexpr int SLOT_SIZE = 16;
    static constexpr int SLOT_STEP = 18;
    static constexpr int BP_ORIGIN_X = 8;
    static constexpr int BP_ORIGIN_Y = 84;
    static constexpr int HB_INV_Y = 142;
    // 背包页 2×2 合成区
    static constexpr int INV_CRAFT_X = 98;
    static constexpr int INV_CRAFT_Y0 = 18;
    static constexpr int INV_CRAFT_Y1 = 36;
    static constexpr int INV_OUTPUT_X = 154;
    static constexpr int INV_OUTPUT_Y = 28;
    // 盔甲槽位（原生坐标）
    static constexpr int ARMOR_X = 8;
    static constexpr int ARMOR_Y0 = 8;   // 头盔
    static constexpr int ARMOR_Y1 = 26;  // 胸甲
    static constexpr int ARMOR_Y2 = 44;  // 护腿
    static constexpr int ARMOR_Y3 = 62;  // 靴子
    static constexpr int ARMOR_GAP = 2;  // 槽位间距

    // ---- crafting_table.png 的合成区坐标（3×3） ----
    static constexpr int CT_CRAFT_X = 30;
    static constexpr int CT_CRAFT_Y = 17;
    static constexpr int CT_OUTPUT_X = 124;
    static constexpr int CT_OUTPUT_Y = 35;
    static constexpr int CT_OUTPUT_SIZE = 16;  // 输出格 16×16

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

    // ---- 合成模式 ----
    void setCraftMode(CraftMode mode) { m_craftMode = mode; }
    CraftMode craftMode() const { return m_craftMode; }

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
    /** @brief 将手上物品放入槽位（盔甲槽位自动验证类型） */
    bool placeInto(int slotIndex);
    /** @brief 将手上 1 个物品放入槽位（右键用） */
    bool placeOneInto(int slotIndex);
    /** @brief 拖拽时从槽位取 count 个物品累加到手上（类型必须相同） */
    bool addToDrag(int slotIndex, int count);
    bool isDragging()  const { return m_dragging != -1; }
    int  dragBlockType() const { return m_dragType; }
    int  dragCount()    const { return m_dragCount; }
    void cancelDrag();

    // ---- 盔甲槽位 ----
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
