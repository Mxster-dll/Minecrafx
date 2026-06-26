#pragma once

#include "../core/linalg.h"
#include "world.h"
#include <windows.h>
#include <vector>
#include <unordered_map>

struct Vec3
{
    double x, z, w;

    Vec3() : x(0.0), z(0.0), w(0.0) {}
    Vec3(double x, double z, double w) : x(x), z(z), w(w) {}

    static Vec3 fromVec4(const Vec4 &v) { return Vec3(v.x, v.z, v.w); }
};

Vec3  vec3Sub(const Vec3 &a, const Vec3 &b);
Vec3  vec3Scale(const Vec3 &v, double s);
double vec3Dot(const Vec3 &a, const Vec3 &b);
Vec3  vec3Cross(const Vec3 &a, const Vec3 &b);
double vec3Length(const Vec3 &v);
double vec3LengthSq(const Vec3 &v);
Vec3  vec3Normalize(const Vec3 &v);

struct Plane2D
{
    Vec3 n;
    Vec3 p;
    Vec3 q;
    double offset;

    Plane2D() : n(0, 0, 1), p(1, 0, 0), q(0, 1, 0), offset(0.0) {}

    static Plane2D fromNormal(const Vec3 &normal, const Vec3 &pRef, double off = 0.0);

    void project(const Vec3 &point, double &u, double &v) const;

};

struct PolyOnPlane
{
    std::vector<double> u;
    std::vector<double> v;
    std::vector<double> ox, oz, ow;
    int n;

    PolyOnPlane() : n(0) {}
    bool valid() const { return n >= 3; }
};

PolyOnPlane intersectCubePlane(
    double x0, double x1,
    double z0, double z1,
    double w0, double w1,
    const Plane2D &plane);

struct Tri3D
{
    double u[3], v[3], y[3];
    double tu[3], tv[3];
    COLORREF color;
    double depth;
    int texId;
    int destroyStage = -1;
};

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
    std::unordered_map<IVec4, size_t> blockIndex;
    Vec4 camRef4D;
    Plane2D plane;
    bool valid = false;
};

Map3D generateMap3D(const class World &world, const class Camera4D &cam4D,
    double blockHalf, COLORREF(*getColor)(int, int, int, int));

void map3DUpdateBlock(Map3D &map, const IVec4 &worldPos, int blockType,
    const Camera4D &cam4D, double blockHalf,
    COLORREF(*getColor)(int, int, int, int));

struct Camera3D
{
    double posU, posV, posY;
    double dirU, dirV, dirY;
    double fov;
    double nearPlane, farPlane;

    Camera3D();
};

bool project3D(double u, double v, double y,
    const Camera3D &cam,
    int sw, int sh,
    int &sx, int &sy, double &depth);