#include "superblock.h"

void SuperBlock::generate(World &world) const
{
    int bx = m_pos.x * SIZE;
    int by = m_pos.y * SIZE;
    int bz = m_pos.z * SIZE;
    int bw = m_pos.w * SIZE;

    for (int x = 0; x < SIZE; ++x)
        for (int y = 0; y < SIZE; ++y)
            for (int z = 0; z < SIZE; ++z)
                for (int w = 0; w < SIZE; ++w)
                    world.set(IVec4(bx + x, by + y, bz + z, bw + w), 1);
}
