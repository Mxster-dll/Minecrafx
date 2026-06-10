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
    for (int x = -4; x <= 3; ++x)
        for (int z = -4; z <= 3; ++z)
            world.set(IVec4(x, 0, z, 0), 1);
}

void generateHypercubeShell(World &world,
    int cx, int cy, int cz, int cw,
    int halfSize)
{
    int r = halfSize;
    for (int x = cx - r; x <= cx + r; ++x)
    {
        for (int y = cy - r; y <= cy + r; ++y)
        {
            for (int z = cz - r; z <= cz + r; ++z)
            {
                for (int w = cw - r; w <= cw + r; ++w)
                {
                    int dx = (x > cx) ? (x - cx) : (cx - x);
                    int dy = (y > cy) ? (y - cy) : (cy - y);
                    int dz = (z > cz) ? (z - cz) : (cz - z);
                    int dw = (w > cw) ? (w - cw) : (cw - w);

                    int dist = dx;
                    if (dy > dist) dist = dy;
                    if (dz > dist) dist = dz;
                    if (dw > dist) dist = dw;

                    if (dist == r)
                        world.set(IVec4(x, y, z, w), 1);
                }
            }
        }
    }
}
