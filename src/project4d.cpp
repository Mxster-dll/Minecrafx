#include "project4d.h"
#include "world.h"
#include "camera.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

// ============================================================================
// Vec3 运算
// ============================================================================

Vec3 vec3Add(const Vec3 &a, const Vec3 &b)
{
    return Vec3(a.x + b.x, a.z + b.z, a.w + b.w);
}

Vec3 vec3Sub(const Vec3 &a, const Vec3 &b)
{
    return Vec3(a.x - b.x, a.z - b.z, a.w - b.w);
}

Vec3 vec3Scale(const Vec3 &v, double s)
{
    return Vec3(v.x * s, v.z * s, v.w * s);
}

double vec3Dot(const Vec3 &a, const Vec3 &b)
{
    return a.x * b.x + a.z * b.z + a.w * b.w;
}

Vec3 vec3Cross(const Vec3 &a, const Vec3 &b)
{
    // xzw 空间中的外积：(x,z,w) × (x',z',w')
    // 按标准 3D 叉积公式，分量置换为 (x,z,w)
    return Vec3(
        a.z * b.w - a.w * b.z,   // x 分量 = yz - zy → az*bw - aw*bz
        a.w * b.x - a.x * b.w,   // z 分量 = zx - xz → aw*bx - ax*bw  (注意: 原本 y 分量)
        a.x * b.z - a.z * b.x    // w 分量 = xy - yx → ax*bz - az*bx  (注意: 原本 z 分量)
    );
}

double vec3LengthSq(const Vec3 &v)
{
    return v.x * v.x + v.z * v.z + v.w * v.w;
}

double vec3Length(const Vec3 &v)
{
    return std::sqrt(vec3LengthSq(v));
}

Vec3 vec3Normalize(const Vec3 &v)
{
    double len = vec3Length(v);
    if (len < 1e-12) return Vec3(0, 0, 1);  // 退化时返回默认方向
    double inv = 1.0 / len;
    return Vec3(v.x * inv, v.z * inv, v.w * inv);
}

// ============================================================================
// Plane2D
// ============================================================================

Plane2D Plane2D::fromNormal(const Vec3 &normal, const Vec3 &pRef, double off)
{
    Plane2D plane;
    plane.n = vec3Normalize(normal);
    plane.offset = off;

    // p = pRef 去掉 n 方向分量后归一化
    double dot = vec3Dot(pRef, plane.n);
    Vec3 pRaw = vec3Sub(pRef, vec3Scale(plane.n, dot));
    double pLen = vec3Length(pRaw);
    if (pLen < 1e-12)
    {
        // pRef 与 n 平行，构造一个垂直于 n 的任意向量
        // 选与 n 差异最大的分量方向
        if (std::abs(plane.n.x) < 0.9)
            pRaw = Vec3(1, 0, 0);
        else
            pRaw = Vec3(0, 1, 0);
        pRaw = vec3Sub(pRaw, vec3Scale(plane.n, vec3Dot(pRaw, plane.n)));
        pRaw = vec3Normalize(pRaw);
    }
    else
    {
        pRaw = vec3Scale(pRaw, 1.0 / pLen);
    }
    plane.p = pRaw;

    // q = n × p
    plane.q = vec3Cross(plane.n, plane.p);

    return plane;
}

void Plane2D::project(const Vec3 &point, double &u, double &v) const
{
    u = vec3Dot(point, p);
    v = vec3Dot(point, q);
}

void Plane2D::rotate(double angle, const Vec3 &axis)
{
    // Rodrigues 旋转公式：v' = v cosθ + (k×v) sinθ + k (k·v) (1-cosθ)
    auto rodrigues = [&](const Vec3 &v) -> Vec3
    {
        double c = std::cos(angle);
        double s = std::sin(angle);
        double dot = vec3Dot(axis, v);
        Vec3 cross = vec3Cross(axis, v);
        return Vec3(
            v.x * c + cross.x * s + axis.x * dot * (1.0 - c),
            v.z * c + cross.z * s + axis.z * dot * (1.0 - c),
            v.w * c + cross.w * s + axis.w * dot * (1.0 - c)
        );
    };

    n = rodrigues(n);
    p = rodrigues(p);
    // q 重新计算保证正交
    q = vec3Cross(n, p);
}

// ============================================================================
// 立方体-平面求交
// ============================================================================

PolyOnPlane intersectCubePlane(
    double x0, double x1,
    double z0, double z1,
    double w0, double w1,
    const Plane2D &plane)
{
    PolyOnPlane result;

    // 立方体 8 个顶点
    struct { double x, z, w; } verts[8] = {
        {x0, z0, w0}, {x1, z0, w0}, {x0, z1, w0}, {x1, z1, w0},
        {x0, z0, w1}, {x1, z0, w1}, {x0, z1, w1}, {x1, z1, w1}
    };

    // 计算每个顶点到平面的有符号距离 d = n·v - offset
    double sd[8];
    for (int i = 0; i < 8; ++i)
    {
        sd[i] = plane.n.x * verts[i].x + plane.n.z * verts[i].z + plane.n.w * verts[i].w - plane.offset;
    }

    // 12 条边（连接索引）
    struct { int a, b; } edges[12] = {
        {0,1}, {2,3}, {4,5}, {6,7}, // x 方向
        {0,2}, {1,3}, {4,6}, {5,7}, // z 方向
        {0,4}, {1,5}, {2,6}, {3,7}  // w 方向
    };

    // 收集交点
    double uArr[12], vArr[12];
    double oxArr[12], ozArr[12], owArr[12];
    int count = 0;

    for (int e = 0; e < 12; ++e)
    {
        int a = edges[e].a, b = edges[e].b;
        double da = sd[a], db = sd[b];

        // 同侧无交点
        if ((da > 1e-12 && db > 1e-12) || (da < -1e-12 && db < -1e-12))
            continue;

        // 线性插值求交点
        double t;
        if (std::abs(da - db) < 1e-12)
        {
            t = 0.5;  // 退化：边在平面上
        }
        else
        {
            t = da / (da - db);
            // 限制在 [0,1] 内
            if (t < 0.0) t = 0.0;
            if (t > 1.0) t = 1.0;
        }

        Vec3 pt(
            verts[a].x + t * (verts[b].x - verts[a].x),
            verts[a].z + t * (verts[b].z - verts[a].z),
            verts[a].w + t * (verts[b].w - verts[a].w)
        );

        // 投影到平面坐标
        double u, v;
        plane.project(pt, u, v);

        uArr[count] = u;
        vArr[count] = v;
        oxArr[count] = pt.x;
        ozArr[count] = pt.z;
        owArr[count] = pt.w;
        ++count;
    }

    if (count < 3) return result;

    // 按角度排序顶点（绕中心逆时针）
    double cu = 0, cv = 0;
    for (int i = 0; i < count; ++i) { cu += uArr[i]; cv += vArr[i]; }
    cu /= count; cv /= count;

    int idx[12];
    double ang[12];
    for (int i = 0; i < count; ++i)
    {
        idx[i] = i;
        ang[i] = std::atan2(vArr[i] - cv, uArr[i] - cu);
    }
    std::sort(idx, idx + count, [&](int a, int b) { return ang[a] < ang[b]; });

    // 排序后线性去重
    int unique = 0;
    for (int i = 0; i < count; ++i)
    {
        int ii = idx[i];
        if (unique > 0)
        {
            int prev = idx[unique - 1];
            double du = uArr[ii] - uArr[prev];
            double dv = vArr[ii] - vArr[prev];
            if (du * du + dv * dv < 1e-10) continue;
        }
        idx[unique++] = ii;
    }
    count = unique;
    if (count < 3) return result;

    result.n = count;
    result.u.resize(count);
    result.v.resize(count);
    result.ox.resize(count);
    result.oz.resize(count);
    result.ow.resize(count);
    for (int i = 0; i < count; ++i)
    {
        result.u[i] = uArr[idx[i]];
        result.v[i] = vArr[idx[i]];
        result.ox[i] = oxArr[idx[i]];
        result.oz[i] = ozArr[idx[i]];
        result.ow[i] = owArr[idx[i]];
    }

    return result;
}

// ============================================================================
// Camera3D
// ============================================================================

Camera3D::Camera3D()
    : posU(0.0), posV(0.0), posY(10.0)
    , dirU(0.0), dirV(0.0), dirY(-1.0)
    , fov(1.0472)       // 60°
    , nearPlane(0.001)
    , farPlane(1000.0)
{}

// ============================================================================
// 3D→2D 投影
// ============================================================================

bool project3D(double u, double v, double y,
    const Camera3D &cam,
    int sw, int sh,
    int &sx, int &sy, double &depth)
{
    // 视线方向（调用方保证已归一化）
    double fU = cam.dirU, fV = cam.dirV, fY = cam.dirY;

    // right = forward × worldUp (worldUp = (0,0,1))
    double rU = fV;
    double rV = -fU;
    double rLen = std::sqrt(rU * rU + rV * rV);
    if (rLen < 1e-12) { rU = 1; rV = 0; }
    else { rU /= rLen; rV /= rLen; }

    // up = right × forward
    double upU = rV * fY;
    double upV = -rU * fY;
    double upY = rU * fV - rV * fU;

    // 物体到相机的向量
    double dU = u - cam.posU;
    double dV = v - cam.posV;
    double dY = y - cam.posY;

    // 相机空间坐标
    double camX = rU * dU + rV * dV;
    double camY = upU * dU + upV * dV + upY * dY;
    double camZ = fU * dU + fV * dV + fY * dY;

    if (camZ < cam.nearPlane || camZ > cam.farPlane) return false;

    // 透视投影
    double halfH = std::tan(cam.fov * 0.5) * camZ;
    double halfW = halfH * static_cast<double>(sw) / static_cast<double>(sh);

    double ndcX = camX / halfW;
    double ndcY = camY / halfH;

    sx = static_cast<int>((ndcX * 0.5 + 0.5) * sw);
    sy = static_cast<int>((-ndcY * 0.5 + 0.5) * sh);
    depth = camZ;

    return (sx >= -sw && sx < sw * 2 && sy >= -sh && sy < sh * 2);
}

// ============================================================================
// 3D 地图生成
// ============================================================================

/** @brief 为单个方块计算棱柱和 AABB，不可见时 prism.u 为空 */
static bool computeBlockPrism(int bx, int by, int bz, int bw,
    const Vec4 &camPos, double sp, double blockHalf, const Plane2D &camPlane,
    COLORREF(*getColor)(int, int, int, int),
    Prism3D &outPrism, Map3D::AABB &outAABB)
{
    double x0 = bx * sp - camPos.x - blockHalf;
    double x1 = bx * sp - camPos.x + blockHalf;
    double z0 = bz * sp - camPos.z - blockHalf;
    double z1 = bz * sp - camPos.z + blockHalf;
    double w0 = bw * sp - camPos.w - blockHalf;
    double w1 = bw * sp - camPos.w + blockHalf;

    PolyOnPlane poly = intersectCubePlane(x0, x1, z0, z1, w0, w1, camPlane);
    if (!poly.valid()) return false;

    outPrism.u = poly.u; outPrism.v = poly.v;
    outPrism.yLow = by * sp - camPos.y - blockHalf;
    outPrism.yHigh = by * sp - camPos.y + blockHalf;
    outPrism.color = getColor(bx, by, bz, bw);

    outAABB.uMin = outAABB.uMax = poly.u[0];
    outAABB.vMin = outAABB.vMax = poly.v[0];
    for (int k = 1; k < poly.n; ++k)
    {
        if (poly.u[k] < outAABB.uMin) outAABB.uMin = poly.u[k];
        if (poly.u[k] > outAABB.uMax) outAABB.uMax = poly.u[k];
        if (poly.v[k] < outAABB.vMin) outAABB.vMin = poly.v[k];
        if (poly.v[k] > outAABB.vMax) outAABB.vMax = poly.v[k];
    }
    outAABB.yMin = outPrism.yLow;
    outAABB.yMax = outPrism.yHigh;
    return true;
}

Map3D generateMap3D(const World &world, const Camera4D &cam4D,
    double blockHalf, COLORREF(*getColor)(int, int, int, int))
{
    Map3D map;
    map.valid = false;
    map.camRef4D = cam4D.getPos();
    map.plane = cam4D.getViewPlane();

    const Vec4 &camPos = cam4D.getPos();
    double sp = blockHalf * 2.0;
    Plane2D camPlane = map.plane;
    camPlane.offset = 0.0;

    // ---- Phase 1: 收集通过廉价预判的候选方块 ----
    struct Candidate { int bx, by, bz, bw; };
    std::vector<Candidate> candidates;
    candidates.reserve(20000);

    for (const auto &[chunkPos, chunk] : world.getChunks())
    {
        if (chunk.empty()) continue;
        // Chunk 级快测
        {
            int sz = World::CHUNK_SIZE;
            double cx0 = (chunk.cx() * sz) * sp - camPos.x - blockHalf;
            double cx1 = (chunk.cx() * sz + sz - 1) * sp - camPos.x + blockHalf;
            double cz0 = (chunk.cz() * sz) * sp - camPos.z - blockHalf;
            double cz1 = (chunk.cz() * sz + sz - 1) * sp - camPos.z + blockHalf;
            double cw0 = (chunk.cw() * sz) * sp - camPos.w - blockHalf;
            double cw1 = (chunk.cw() * sz + sz - 1) * sp - camPos.w + blockHalf;
            double cmin = 0, cmax = 0;
            double nx = camPlane.n.x, nz = camPlane.n.z, nw = camPlane.n.w;
            if (nx > 0) { cmin += nx * cx0; cmax += nx * cx1; }
            else { cmin += nx * cx1; cmax += nx * cx0; }
            if (nz > 0) { cmin += nz * cz0; cmax += nz * cz1; }
            else { cmin += nz * cz1; cmax += nz * cz0; }
            if (nw > 0) { cmin += nw * cw0; cmax += nw * cw1; }
            else { cmin += nw * cw1; cmax += nw * cw0; }
            if (cmin > 0.0 || cmax < 0.0) continue;
        }
        for (const auto &[localPos, type] : chunk.blocks())
        {
            IVec4 blk = chunk.localToWorld(localPos.x, localPos.y, localPos.z, localPos.w);
            int bx = blk.x, by = blk.y, bz = blk.z, bw = blk.w;
            // 方块级廉价预判
            double x0 = bx * sp - camPos.x - blockHalf;
            double x1 = bx * sp - camPos.x + blockHalf;
            double z0 = bz * sp - camPos.z - blockHalf;
            double z1 = bz * sp - camPos.z + blockHalf;
            double w0 = bw * sp - camPos.w - blockHalf;
            double w1 = bw * sp - camPos.w + blockHalf;
            double cmin = 0, cmax = 0;
            double nx = camPlane.n.x, nz = camPlane.n.z, nw = camPlane.n.w;
            if (nx > 0) { cmin += nx * x0; cmax += nx * x1; }
            else { cmin += nx * x1; cmax += nx * x0; }
            if (nz > 0) { cmin += nz * z0; cmax += nz * z1; }
            else { cmin += nz * z1; cmax += nz * z0; }
            if (nw > 0) { cmin += nw * w0; cmax += nw * w1; }
            else { cmin += nw * w1; cmax += nw * w0; }
            if (cmin > 0.0 || cmax < 0.0) continue;
            candidates.push_back({ bx, by, bz, bw });
        }
    }

    // ---- Phase 2: OpenMP 并行求交 ----
    size_t N = candidates.size();
    int nThreads = 1;
#ifdef _OPENMP
    nThreads = omp_get_max_threads();
    if (nThreads > 16) nThreads = 16;
    if (N < 64) nThreads = 1;
#endif

    std::vector<std::vector<Prism3D>> threadPrisms(nThreads);
    std::vector<std::vector<Map3D::AABB>> threadAABBs(nThreads);
    std::vector<std::vector<IVec4>> threadPos(nThreads);  // 每个棱柱对应的世界坐标

#pragma omp parallel if(nThreads > 1) num_threads(nThreads)
    {
        int tid = 0;
#ifdef _OPENMP
        tid = omp_get_thread_num();
#endif
        auto &prisms = threadPrisms[tid];
        auto &aabbs = threadAABBs[tid];
        auto &pos = threadPos[tid];
        prisms.reserve(N / nThreads + 64);
        aabbs.reserve(N / nThreads + 64);
        pos.reserve(N / nThreads + 64);

#pragma omp for schedule(static)
        for (size_t i = 0; i < N; ++i)
        {
            auto &c = candidates[i];
            Prism3D p; Map3D::AABB ab;
            if (!computeBlockPrism(c.bx, c.by, c.bz, c.bw,
                camPos, sp, blockHalf, camPlane, getColor, p, ab))
                continue;
            prisms.push_back(p);
            aabbs.push_back(ab);
            pos.push_back(IVec4(c.bx, c.by, c.bz, c.bw));
        }
    }

    // ---- Phase 3: 合并 ----
    size_t total = 0;
    for (auto &tp : threadPrisms) total += tp.size();
    map.prisms.reserve(total);
    map.aabbs.reserve(total);
    map.blockIndex.reserve(total);
    for (int t = 0; t < nThreads; ++t)
    {
        size_t base = map.prisms.size();
        map.prisms.insert(map.prisms.end(), threadPrisms[t].begin(), threadPrisms[t].end());
        map.aabbs.insert(map.aabbs.end(), threadAABBs[t].begin(), threadAABBs[t].end());
        for (size_t i = 0; i < threadPos[t].size(); ++i)
            map.blockIndex[threadPos[t][i]] = base + i;
    }

    map.valid = true;
    return map;
}

// ============================================================================
// 增量更新单个方块
// ============================================================================

void map3D_updateBlock(Map3D &map, const IVec4 &worldPos, int blockType,
    const Camera4D &cam4D, double blockHalf,
    COLORREF(*getColor)(int, int, int, int))
{
    if (!map.valid) return;

    // ---- 移除旧棱柱 ----
    auto it = map.blockIndex.find(worldPos);
    if (it != map.blockIndex.end())
    {
        size_t idx = it->second;
        size_t last = map.prisms.size() - 1;

        if (idx != last)
        {
            // 用最后一个替换被删除项
            map.prisms[idx] = std::move(map.prisms[last]);
            map.aabbs[idx] = map.aabbs[last];
            // 更新被移动方块的索引：需要找到它对应的 worldPos
            // 遍历 blockIndex 找到 value==last 的 key
            for (auto &[pos, i] : map.blockIndex)
                if (i == last) { i = idx; break; }
        }
        map.prisms.pop_back();
        map.aabbs.pop_back();
        map.blockIndex.erase(it);
    }

    // ---- 添加新方块（type>0） ----
    if (blockType > 0)
    {
        // 必须使用 camRef4D 而非当前摄像机位置——地图中所有棱柱共用一个局部坐标系原点
        const Vec4 &camPos = map.camRef4D;
        double sp = blockHalf * 2.0;
        Plane2D camPlane = map.plane;
        camPlane.offset = 0.0;

        Prism3D p; Map3D::AABB ab;
        if (computeBlockPrism(worldPos.x, worldPos.y, worldPos.z, worldPos.w,
            camPos, sp, blockHalf, camPlane, getColor, p, ab))
        {
            size_t idx = map.prisms.size();
            map.prisms.push_back(p);
            map.aabbs.push_back(ab);
            map.blockIndex[worldPos] = idx;
        }
    }
}


