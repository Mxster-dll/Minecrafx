#pragma once

#include <unordered_map>
#include <functional>

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

namespace std
{
    template <>
    struct hash<IVec4>
    {
        size_t operator()(const IVec4 &v) const noexcept
        {

            size_t h = 0;
            h ^= std::hash<int>{}(v.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(v.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(v.w) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
}

class Chunk
{
public:
    static constexpr int SIZE = 16;

    Chunk() = default;
    explicit Chunk(int cx, int cy, int cz, int cw)
        : m_cx(cx), m_cy(cy), m_cz(cz), m_cw(cw)
    {}

    int get(int lx, int ly, int lz, int lw) const;
    void set(int lx, int ly, int lz, int lw, int type);

    bool empty() const { return m_blocks.empty(); }
    size_t blockCount() const { return m_blocks.size(); }
    const std::unordered_map<IVec4, int> &blocks() const { return m_blocks; }

    int cx() const { return m_cx; }
    int cy() const { return m_cy; }
    int cz() const { return m_cz; }
    int cw() const { return m_cw; }

    IVec4 localToWorld(int lx, int ly, int lz, int lw) const
    {
        return IVec4(m_cx * SIZE + lx, m_cy * SIZE + ly,
            m_cz * SIZE + lz, m_cw * SIZE + lw);
    }

private:
    int m_cx = 0, m_cy = 0, m_cz = 0, m_cw = 0;
    std::unordered_map<IVec4, int> m_blocks;
};

class World
{
public:
    static constexpr int CHUNK_SIZE = Chunk::SIZE;

    World() = default;

    int get(const IVec4 &pos) const;
    void set(const IVec4 &pos, int type);
    void clear() { m_chunks.clear(); }
    size_t totalBlocks() const;

    const std::unordered_map<IVec4, Chunk> &getChunks() const { return m_chunks; }

    static void worldToChunk(int wx, int &cx, int &lx);
    static void worldToChunk(const IVec4 &wpos, IVec4 &cpos, IVec4 &lpos);

private:
    std::unordered_map<IVec4, Chunk> m_chunks;
};