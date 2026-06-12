#pragma once

#include "world.h"

/**
 * @brief 超方块 — 世界构建最小单位，由 16⁴ 个子方块组成
 *
 * 每个超方块占据 16×16×16×16 的网格空间，
 * 实际子方块坐标 = pos * 16 + [0..15]。
 */
class SuperBlock
{
public:
    static constexpr int SIZE = 16;

    /**
     * @param pos 超方块在网格中的坐标（以超方块为单位，16 个子方块跨度）
     */
    explicit SuperBlock(const IVec4 &pos) : m_pos(pos) {}

    /** @brief 将 16⁴ 个子方块写入世界 */
    void generate(World &world) const;

    const IVec4 &pos() const { return m_pos; }

private:
    IVec4 m_pos;
};
