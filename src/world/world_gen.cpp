#include "world_gen.h"
#include "world.h"
#include "../core/constant.h"
#include "../game/inventory.h"
#include <cmath>
#include <cstdlib>

// TODO 换柏林噪声实现随机化，后续添加群系

int terrainHeight(int x, int z, int w)
{
    double h = 0.0;
    h += std::sin(x * 0.25) * std::cos(z * 0.30) * 6.0;
    h += std::cos(x * 0.45 + 1.2) * std::sin(z * 0.55) * 4.0;
    h += std::sin((x + z) * 0.35) * 3.0;
    h += std::cos(w * 0.40) * std::sin(x * 0.50 + z * 0.30) * 2.5;
    h += std::sin(x * 0.70 - z * 0.60) * std::cos(w * 0.50) * 2.0;
    h += 5.0;
    return (int) std::floor(h);
}

void generateSurvivalWorld(World &world, int MX, int MZ, int MW)
{

    for (int x = 0; x < MX; ++x)
        for (int z = 0; z < MZ; ++z)
            for (int w = 0; w < MW; ++w)
            {
                int ht = terrainHeight(x, z, w);
                if (ht < 1) ht = 1;
                for (int y = 0; y < ht; ++y)
                {
                    int type = BLOCK_DIRT;
                    if (y == ht - 1) type = BLOCK_GRASS;
                    else if (y < ht - 3) type = BLOCK_STONE;
                    world.set(IVec4(x, y, z, w), type);
                }
            }

    srand(42);
    for (int i = 0; i < 120; ++i)
    {
        int tx = rand() % (MX - 2) + 1, tz = rand() % (MZ - 2) + 1, tw = rand() % (MW - 2) + 1;
        int groundY = terrainHeight(tx, tz, tw);
        if (groundY < 2) continue;
        int trunkH = 3 + rand() % 3;
        for (int ty = groundY; ty < groundY + trunkH; ++ty)
            world.set(IVec4(tx, ty, tz, tw), BLOCK_LOG);
        int leafBase = groundY + trunkH - 1;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz)
                for (int dw = -1; dw <= 1; ++dw)
                {
                    int lx = tx + dx, lz = tz + dz, lw = tw + dw;
                    if (lx < 0 || lx >= MX || lz < 0 || lz >= MZ || lw < 0 || lw >= MW) continue;
                    if (dx == 0 && dz == 0 && dw == 0) continue;
                    if ((dx != 0) + (dz != 0) + (dw != 0) >= 2 && (rand() % 3) != 0) continue;
                    for (int ly = leafBase; ly <= leafBase + 2; ++ly)
                        if (ly < 20) world.set(IVec4(lx, ly, lz, lw), BLOCK_LEAVES);
                }
    }

    srand(12345);
    for (int i = 0; i < 800; ++i)
    {
        int ox = rand() % MX, oz = rand() % MZ, ow = rand() % MW;
        int oy = rand() % 5 + 1;
        int oreType = BLOCK_COAL_ORE;
        int r = rand() % 100;
        if (r < 50) oreType = BLOCK_COAL_ORE;
        else if (r < 80) oreType = BLOCK_IRON_ORE;
        else if (r < 95) oreType = BLOCK_GOLD_ORE;
        else oreType = BLOCK_DIAMOND_ORE;
        int clusterSize = 2 + rand() % 4;
        for (int j = 0; j < clusterSize; ++j)
        {
            int dx = (rand() % 3) - 1, dy = (rand() % 3) - 1, dz = (rand() % 3) - 1, dw = (rand() % 3) - 1;
            int nx = ox + dx, ny = oy + dy, nz = oz + dz, nw = ow + dw;
            if (nx >= 0 && nx < MX && nz >= 0 && nz < MZ && nw >= 0 && nw < MW && ny >= 0 && ny < terrainHeight(nx, nz, nw) - 1)
                if (world.get(IVec4(nx, ny, nz, nw)) == BLOCK_DIRT || world.get(IVec4(nx, ny, nz, nw)) == BLOCK_STONE)
                    world.set(IVec4(nx, ny, nz, nw), oreType);
        }
    }

    for (int i = 0; i < 600; ++i)
    {
        int ox = rand() % MX, oz = rand() % MZ, ow = rand() % MW;
        int oy = rand() % 3;
        int clusterSize = 3 + rand() % 5;
        for (int j = 0; j < clusterSize; ++j)
        {
            int dx = (rand() % 3) - 1, dy = (rand() % 3) - 1, dz = (rand() % 3) - 1, dw = (rand() % 3) - 1;
            int nx = ox + dx, ny = oy + dy, nz = oz + dz, nw = ow + dw;
            if (nx >= 0 && nx < MX && nz >= 0 && nz < MZ && nw >= 0 && nw < MW && ny >= 0 && ny < terrainHeight(nx, nz, nw) - 1)
                if (world.get(IVec4(nx, ny, nz, nw)) == BLOCK_STONE)
                    world.set(IVec4(nx, ny, nz, nw), BLOCK_COAL_ORE);
        }
    }
}

void generateCreativeWorld(World &world, int CX, int CZ, int CW)
{
    for (int x = 0; x < CX; ++x)
        for (int z = 0; z < CZ; ++z)
            for (int w = 0; w < CW; ++w)
                world.set(IVec4(x, 0, z, w), BLOCK_GRASS);
}

void initSurvivalInventory(Inventory &inv)
{
    for (int i = 0; i < Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS; ++i)
        inv.getSlot(i) = { BLOCK_AIR, 0 };
    inv.getSlot(0) = { BLOCK_IRON_PICKAXE, 1 };
}

void initCreativeInventory(Inventory &inv)
{
    auto isExcluded = [](int id) -> bool
    {

        if (id >= 22 && id <= 54) return true;

        if (id >= 37 && id <= 40) return true;
        if (id >= 46 && id <= 49) return true;
        if (id >= 55 && id <= 58) return true;
        return false;
    };
    int slot = 0;
    for (int id = 1; id < MAX_BLOCK_TYPE && slot < Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS; ++id)
    {
        if (!isExcluded(id))
            inv.getSlot(slot++) = { id, 1 };
    }
}