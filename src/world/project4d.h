#pragma once

#include "../core/linalg.h"
#include "world.h"
#include <windows.h>
#include <vector>
#include <unordered_map>

// ============================================================================
// Vec3 — xzw 子空间中的三维向量（x, z, w）
// ============================================================================
struct Vec3
{
    double x, z, w;

    Vec3() : x(0.0), z(0.0), w(0.0) {}
    Vec3(double x, double z, double w) : x(x), z(z), w(w) {}

    /** @brief 从 Vec4 提取 xzw 分量 */
    static Vec3 fromVec4(const Vec4 &v) { return Vec3(v.x, v.z, v.w); }
};

// ---- Vec3 运算 ----
Vec3  vec3Sub(const Vec3 &a, const Vec3 &b);
Vec3  vec3Scale(const Vec3 &v, double s);
double vec3Dot(const Vec3 &a, const Vec3 &b);
Vec3  vec3Cross(const Vec3 &a, const Vec3 &b);   // xzw 空间中的外积
double vec3Length(const Vec3 &v);
double vec3LengthSq(const Vec3 &v);
Vec3  vec3Normalize(const Vec3 &v);

// ============================================================================
// Plane2D — 过原点的 xzw 平面上的二维坐标系
//
// 平面方程：n·(x,z,w) = offset
// 基向量 p, q 满足：p·n = 0, q = n × p, |p| = |q| = 1
// 平面上任一点的坐标为 (u, v) = (点·p, 点·q)
// ============================================================================
struct Plane2D
{
    Vec3 n;       // 平面法向量（单位）
    Vec3 p;       // 第一基向量（单位，在平面上）
    Vec3 q;       // 第二基向量 = n × p（单位）
    double offset; // 平面偏移：n·(x,z,w) = offset

    Plane2D() : n(0, 0, 1), p(1, 0, 0), q(0, 1, 0), offset(0.0) {}

    /**
     * @brief 由法向量和参考向量初始化平面坐标系
     * @param normal   平面法向量（不必单位化）
     * @param pRef     参考向量，其在平面上的分量将作为 p
     * @param off      平面偏移量（相机空间为 0）
     */
    static Plane2D fromNormal(const Vec3 &normal, const Vec3 &pRef, double off = 0.0);

    /**
     * @brief 将 xzw 空间中的点投影到平面坐标系
     * @return (u, v) = (point·p, point·q)
     */
    void project(const Vec3 &point, double &u, double &v) const;

};

// ============================================================================
// 立方体-平面求交
// ============================================================================

/**
 * @brief 平面与轴对齐立方体的交线多边形
 *
 * 立方体在 xzw 空间中的范围：[x0,x1] × [z0,z1] × [w0,w1]
 * 返回交多边形各顶点在平面坐标系中的 (u, v) 坐标，按逆时针排列。
 */
struct PolyOnPlane
{
    std::vector<double> u;  // 顶点 p-坐标
    std::vector<double> v;  // 顶点 q-坐标
    std::vector<double> ox, oz, ow;  // 原始相机空间 xzw 坐标
    int n;                   // 顶点数 (3~6)

    PolyOnPlane() : n(0) {}
    bool valid() const { return n >= 3; }
};

/**
 * @brief 计算轴对齐立方体与平面的交线多边形
 * @return 多边形顶点（平面坐标），若不相交则 n=0
 */
PolyOnPlane intersectCubePlane(
    double x0, double x1,
    double z0, double z1,
    double w0, double w1,
    const Plane2D &plane);

// ============================================================================
// 4D→3D 投影结果
// ============================================================================

/** @brief 3D 空间中的三角形面（用于最终渲染） */
struct Tri3D
{
    double u[3], v[3], y[3];     // 3D 顶点坐标
    double tu[3], tv[3];          // 纹理坐标 (0~1)
    COLORREF color;               // 纯色后备（texId<0 时使用）
    double depth;
    int texId;                    // 纹理 ID: 0~11, -1=纯色
    int destroyStage = -1;        // 挖掘阶段 0~9, -1=无叠加
};

// ============================================================================
// 3D 地图（4D→3D 切片结果，可复用）
// ============================================================================

struct Prism3D
{
    std::vector<double> u, v;
    double yLow, yHigh;
    COLORREF color;
};

struct Map3D
{
    std::vector<Prism3D> prisms;
    struct AABB { double uMin, uMax, vMin, vMax, yMin, yMax; };
    std::vector<AABB> aabbs;
    std::unordered_map<IVec4, size_t> blockIndex;  // 世界坐标 → prisms/aabbs 下标
    Vec4 camRef4D;
    Plane2D plane;
    bool valid = false;
};

Map3D generateMap3D(const class World &world, const class Camera4D &cam4D,
    double blockHalf, COLORREF(*getColor)(int, int, int, int));

/** @brief 增量更新地图中单个方块（type=0 移除，>0 添加/更新） */
void map3D_updateBlock(Map3D &map, const IVec4 &worldPos, int blockType,
    const Camera4D &cam4D, double blockHalf,
    COLORREF(*getColor)(int, int, int, int));

// ============================================================================
// 3D→2D 投影（标准透视投影）
// ============================================================================

/**
 * @brief 3D 透视投影相机
 *
 * 将 (u, v, y) 3D 空间投影到屏幕。
 */
struct Camera3D
{
    double posU, posV, posY;   // 相机位置
    double dirU, dirV, dirY;   // 视线方向（单位向量）
    double fov;                 // 视场角（弧度）
    double nearPlane, farPlane;

    Camera3D();
};

/**
 * @brief 将 3D 点投影到屏幕
 * @param u,v,y  3D 空间坐标
 * @param cam    3D 相机
 * @param sw,sh  屏幕宽高
 * @param sx,sy  输出屏幕坐标
 * @param depth  输出深度值
 * @return 是否在屏幕内
 */
bool project3D(double u, double v, double y,
    const Camera3D &cam,
    int sw, int sh,
    int &sx, int &sy, double &depth);


