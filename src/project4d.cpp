#include "project4d.h"
#include "world.h"
#include "camera.h"
#include <cmath>
#include <algorithm>

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

        // 去重（与已收集的点比较）
        bool dup = false;
        for (int i = 0; i < count; ++i)
        {
            double du = uArr[i] - u, dv = vArr[i] - v;
            if (du * du + dv * dv < 1e-12) { dup = true; break; }
        }
        if (!dup && count < 12)
        {
            uArr[count] = u;
            vArr[count] = v;
            oxArr[count] = pt.x;
            ozArr[count] = pt.z;
            owArr[count] = pt.w;
            ++count;
        }
    }

    if (count < 3) return result;  // 无有效多边形

    // 按角度排序顶点（绕中心逆时针）
    double cu = 0, cv = 0;
    for (int i = 0; i < count; ++i) { cu += uArr[i]; cv += vArr[i]; }
    cu /= count; cv /= count;

    // 索引数组用于排序
    int idx[12];
    double ang[12];
    for (int i = 0; i < count; ++i)
    {
        idx[i] = i;
        ang[i] = std::atan2(vArr[i] - cv, uArr[i] - cu);
    }
    std::sort(idx, idx + count, [&](int a, int b) { return ang[a] < ang[b]; });

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

    for (const auto &[chunkPos, chunk] : world.getChunks())
    {
        if (chunk.empty()) continue;
        for (const auto &[localPos, type] : chunk.blocks())
        {
            IVec4 blk = chunk.localToWorld(localPos.x, localPos.y, localPos.z, localPos.w);
            int bx = blk.x, by = blk.y, bz = blk.z, bw = blk.w;

            // 相机相对 xzw 包围盒
            double x0 = bx * sp - camPos.x - blockHalf;
            double x1 = bx * sp - camPos.x + blockHalf;
            double z0 = bz * sp - camPos.z - blockHalf;
            double z1 = bz * sp - camPos.z + blockHalf;
            double w0 = bw * sp - camPos.w - blockHalf;
            double w1 = bw * sp - camPos.w + blockHalf;

            PolyOnPlane poly = intersectCubePlane(x0, x1, z0, z1, w0, w1, camPlane);
            if (!poly.valid()) continue;

            Prism3D p;
            p.u = poly.u; p.v = poly.v;
            p.yLow = by * sp - camPos.y - blockHalf;
            p.yHigh = by * sp - camPos.y + blockHalf;
            p.color = getColor(bx, by, bz, bw);
            map.prisms.push_back(p);

            // 碰撞 AABB
            Map3D::AABB ab;
            ab.uMin = ab.uMax = poly.u[0];
            ab.vMin = ab.vMax = poly.v[0];
            for (int i = 1; i < poly.n; ++i)
            {
                if (poly.u[i] < ab.uMin) ab.uMin = poly.u[i];
                if (poly.u[i] > ab.uMax) ab.uMax = poly.u[i];
                if (poly.v[i] < ab.vMin) ab.vMin = poly.v[i];
                if (poly.v[i] > ab.vMax) ab.vMax = poly.v[i];
            }
            ab.yMin = p.yLow; ab.yMax = p.yHigh;
            map.aabbs.push_back(ab);
        } // inner chunk blocks
    } // outer chunks

    map.valid = true;
    return map;
}


