#include "superblock.h"
#include <cmath>
#include <functional>

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

// 检查单个子方块 xzw 包围盒是否与平面相交
static bool blockIntersectsPlane(int bx, int bz, int bw,
    const Vec4 &camPos, const Plane2D &plane, double half, double sp)
{
    double x0 = bx * sp - camPos.x - half;
    double x1 = bx * sp - camPos.x + half;
    double z0 = bz * sp - camPos.z - half;
    double z1 = bz * sp - camPos.z + half;
    double w0 = bw * sp - camPos.w - half;
    double w1 = bw * sp - camPos.w + half;

    double minDot = 0.0, maxDot = 0.0;
    double nx = plane.n.x, nz = plane.n.z, nw = plane.n.w;

    if (nx > 0) { minDot += nx * x0; maxDot += nx * x1; }
    else { minDot += nx * x1; maxDot += nx * x0; }
    if (nz > 0) { minDot += nz * z0; maxDot += nz * z1; }
    else { minDot += nz * z1; maxDot += nz * z0; }
    if (nw > 0) { minDot += nw * w0; maxDot += nw * w1; }
    else { minDot += nw * w1; maxDot += nw * w0; }

    return minDot <= 0.0 && maxDot >= 0.0;
}

void SuperBlock::collectVisible(const Vec4 &camPos, const Plane2D &plane,
    double blockHalf, std::vector<IVec4> &out, int &outPreOccl) const
{
    int baseX = m_pos.x * SIZE;
    int baseY = m_pos.y * SIZE;
    int baseZ = m_pos.z * SIZE;
    int baseW = m_pos.w * SIZE;

    double sp = blockHalf * 2.0;
    double overAbsSum = std::abs(plane.n.x) + std::abs(plane.n.z) + std::abs(plane.n.w);

    std::function<void(int, int, int, int, int)> traverse =
        [&](int bx, int by, int bz, int bw, int size)
    {
        double cs = size * sp;
        double cx = (bx + size * 0.5) * sp - camPos.x;
        double cz = (bz + size * 0.5) * sp - camPos.z;
        double cw = (bw + size * 0.5) * sp - camPos.w;
        double cod = plane.n.x * cx + plane.n.z * cz + plane.n.w * cw;
        if (std::abs(cod) > cs * overAbsSum + 1e-9) return;

        if (size == 1)
        {
            if (!blockIntersectsPlane(bx, bz, bw, camPos, plane, blockHalf, sp))
                return;
            ++outPreOccl;

            // 内部方块（8 个方向都在超方块内）不可见
            int lx = bx - baseX, ly = by - baseY;
            int lz = bz - baseZ, lw = bw - baseW;
            if (lx > 0 && lx < SIZE - 1 &&
                ly > 0 && ly < SIZE - 1 &&
                lz > 0 && lz < SIZE - 1 &&
                lw > 0 && lw < SIZE - 1)
                return;

            out.push_back(IVec4(bx, by, bz, bw));
            return;
        }

        int half = size / 2;
        for (int dx = 0; dx < 2; ++dx)
            for (int dy = 0; dy < 2; ++dy)
                for (int dz = 0; dz < 2; ++dz)
                    for (int dw = 0; dw < 2; ++dw)
                        traverse(bx + dx * half, by + dy * half,
                            bz + dz * half, bw + dw * half, half);
    };

    traverse(baseX, baseY, baseZ, baseW, SIZE);
}
