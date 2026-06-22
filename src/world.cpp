#include "world.h"

// ============================================================================
// Chunk
// ============================================================================

int Chunk::get(int lx, int ly, int lz, int lw) const
{
    auto it = m_blocks.find(IVec4(lx, ly, lz, lw));
    return (it != m_blocks.end()) ? it->second : 0;
}

void Chunk::set(int lx, int ly, int lz, int lw, int type)
{
    IVec4 key(lx, ly, lz, lw);
    if (type == 0)
        m_blocks.erase(key);
    else
        m_blocks[key] = type;
}

// ============================================================================
// World
// ============================================================================

static inline int floorDiv(int a, int b)
{
    int q = a / b;
    if (a < 0 && a % b != 0) --q;
    return q;
}

void World::worldToChunk(int wx, int &cx, int &lx)
{
    cx = floorDiv(wx, CHUNK_SIZE);
    lx = wx - cx * CHUNK_SIZE;
}

void World::worldToChunk(const IVec4 &wpos, IVec4 &cpos, IVec4 &lpos)
{
    worldToChunk(wpos.x, cpos.x, lpos.x);
    worldToChunk(wpos.y, cpos.y, lpos.y);
    worldToChunk(wpos.z, cpos.z, lpos.z);
    worldToChunk(wpos.w, cpos.w, lpos.w);
}

int World::get(const IVec4 &pos) const
{
    IVec4 cpos, lpos;
    worldToChunk(pos, cpos, lpos);
    auto it = m_chunks.find(cpos);
    if (it == m_chunks.end()) return 0;
    return it->second.get(lpos.x, lpos.y, lpos.z, lpos.w);
}

void World::set(const IVec4 &pos, int type)
{
    IVec4 cpos, lpos;
    worldToChunk(pos, cpos, lpos);
    if (type == 0)
    {
        auto it = m_chunks.find(cpos);
        if (it == m_chunks.end()) return;
        it->second.set(lpos.x, lpos.y, lpos.z, lpos.w, 0);
        if (it->second.empty())
            m_chunks.erase(it);
    }
    else
    {
        auto [it, inserted] = m_chunks.try_emplace(cpos,
            cpos.x, cpos.y, cpos.z, cpos.w);
        it->second.set(lpos.x, lpos.y, lpos.z, lpos.w, type);
    }
}

size_t World::totalBlocks() const
{
    size_t n = 0;
    for (const auto &[k, ch] : m_chunks)
        n += ch.blockCount();
    return n;
}

// ============================================================================
// 世界生成函数
// ============================================================================

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
