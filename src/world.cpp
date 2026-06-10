#include "world.h"

int World::get(const IVec4 &pos) const
{
    auto it = m_blocks.find(pos);
    if (it != m_blocks.end())
        return it->second;
    return 0;  // 空气
}

void World::set(const IVec4 &pos, int type)
{
    if (type == 0)
        m_blocks.erase(pos);  // 删除空气键
    else
        m_blocks[pos] = type;
}

void generateFloor(World &world)
{
    // y=0, w=0 平面，x,z 范围 [-4, 3] 生成 8×8 石板
    for (int x = -4; x <= 3; ++x)
    {
        for (int z = -4; z <= 3; ++z)
        {
            world.set(IVec4(x, 0, z, 0), 1);  // 类型 1 = 石块
        }
    }
}
