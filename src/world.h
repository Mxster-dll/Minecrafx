#pragma once

#include <unordered_map>
#include <functional>

/**
 * @brief 4D 整数坐标（用于方块索引）
 */
struct IVec4
{
    int x, y, z, w;

    IVec4() : x(0), y(0), z(0), w(0) {}
    IVec4(int x, int y, int z, int w) : x(x), y(y), z(z), w(w) {}

    bool operator==(const IVec4 &other) const
    {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
};

/**
 * @brief std::hash 特化，使 IVec4 可用于 unordered_map
 */
namespace std
{
    template <>
    struct hash<IVec4>
    {
        size_t operator()(const IVec4 &v) const noexcept
        {
            // 使用位混合避免简单的 XOR 碰撞
            size_t h = 0;
            h ^= std::hash<int>{}(v.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(v.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(v.w) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
}

/**
 * @brief 世界数据管理类
 *
 * 使用哈希表存储非空方块，类型 0 表示空气（不存在于表中）。
 * 世界规模上限 16×16×16×16。
 */
class World
{
public:
    static constexpr int MAX_WORLD_SIZE = 16;

    World() = default;

    /**
     * @brief 获取指定坐标的方块类型
     * @return 方块类型，0 表示空气（空）
     */
    int get(const IVec4 &pos) const;

    /**
     * @brief 设置指定坐标的方块类型
     * @param type 方块类型，0 表示删除（移除空气方块键）
     */
    void set(const IVec4 &pos, int type);

    /**
     * @brief 获取所有非空方块的只读引用（用于遍历渲染）
     */
    const std::unordered_map<IVec4, int> &getAllBlocks() const { return m_blocks; }

    /** @brief 清空世界 */
    void clear() { m_blocks.clear(); }

private:
    std::unordered_map<IVec4, int> m_blocks;
};

// ============================================================================
// 世界生成函数
// ============================================================================

/**
 * @brief 生成初始地板：y=0, w=0 平面的 8×8 石块
 * @param world 目标世界对象
 *
 * 在 y=0, w=0 平面，x 和 z 范围 [-4, 3] 生成石块（类型 1）。
 */
void generateFloor(World &world);
