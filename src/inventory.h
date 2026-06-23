#pragma once

#include "constant.h"

// ============================================================================
// Inventory — 背包 + 快捷栏（共 36 格），支持拖拽
// ============================================================================
class Inventory
{
public:
    static constexpr int HOTBAR_SLOTS = 9;
    static constexpr int BACKPACK_ROWS = 3;
    static constexpr int BACKPACK_COLS = 9;
    static constexpr int BACKPACK_SLOTS = BACKPACK_ROWS * BACKPACK_COLS;
    static constexpr int TOTAL_SLOTS = HOTBAR_SLOTS + BACKPACK_SLOTS;

    // 槽位在 inventory.png 原生坐标中的参数
    static constexpr int SLOT_SIZE = 16;   // 格子原生大小
    static constexpr int SLOT_STEP = 18;   // 格子间距
    static constexpr int BP_ORIGIN_X = 8;    // 背包左上角 x
    static constexpr int BP_ORIGIN_Y = 84;   // 背包第一排 y
    static constexpr int HB_INV_Y = 142;  // 快捷栏在背包页的 y

    struct Slot
    {
        int blockType = BLOCK_AIR;
        int count = 0;
    };

    Inventory();

    // ---- 访问 ----
    Slot &getSlot(int index);
    const Slot &getSlot(int index) const;

    /** @brief 快捷栏块类型（用于放置方块） */
    int hotbarBlockType(int hotbarIndex) const;

    /** @brief 快捷栏设置 */
    void setHotbar(int hotbarIndex, int blockType, int count);

    // ---- 坐标换算 ----
    /** @brief 获取第 index 个槽位在屏幕上的矩形
     *  @param imgDispX/Y  inventory 图片的屏幕左上角
     *  @param imgDispW/H  inventory 图片的屏幕显示宽高
     *  @param imgNativeW/H inventory 图片的原生宽高（1x） */
    void slotScreenRect(int index,
        int imgDispX, int imgDispY, int imgDispW, int imgDispH,
        int imgNativeW, int imgNativeH,
        int &outX, int &outY, int &outW, int &outH) const;

    /** @brief 根据屏幕坐标命中槽位，返回索引，-1 未命中 */
    int hitTest(int screenX, int screenY,
        int imgDispX, int imgDispY, int imgDispW, int imgDispH,
        int imgNativeW, int imgNativeH) const;

    // ---- 拖拽 ----
    /** @brief 开始拖拽（记录拿起的东西），成功返回 true */
    bool pickup(int slotIndex);

    /** @brief 放入槽位，返回是否成功（同种物品叠加或空格放入） */
    bool placeInto(int slotIndex);

    /** @brief 是否正在拖拽 */
    bool isDragging() const { return m_dragging != -1; }

    /** @brief 获取拖拽中的物品信息 */
    int dragBlockType() const { return m_dragType; }
    int dragCount()    const { return m_dragCount; }

    /** @brief 取消拖拽（把物品放回原槽位） */
    void cancelDrag();

private:
    Slot m_slots[TOTAL_SLOTS];

    // 拖拽状态
    int m_dragging = -1;   // 源槽位索引
    int m_dragType = BLOCK_AIR;
    int m_dragCount = 0;
};
