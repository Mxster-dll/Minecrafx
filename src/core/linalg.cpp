#include "linalg.h"
#include "../world/camera.h"   // 用于 project() 实现

// ============================================================================
// Vec4 向量运算
// ============================================================================

Vec4 vec4Add(const Vec4 &a, const Vec4 &b)
{
    return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

Vec4 vec4Sub(const Vec4 &a, const Vec4 &b)
{
    return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

Vec4 vec4Scale(const Vec4 &v, double s)
{
    return Vec4(v.x * s, v.y * s, v.z * s, v.w * s);
}

double vec4Dot(const Vec4 &a, const Vec4 &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

double vec4Length(const Vec4 &v)
{
    return std::sqrt(vec4LengthSq(v));
}

double vec4LengthSq(const Vec4 &v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}

Vec4 vec4Normalize(const Vec4 &v)
{
    double len = vec4Length(v);
    if (len < 1e-12)
        return Vec4();  // 零向量返回零向量
    double inv = 1.0 / len;
    return Vec4(v.x * inv, v.y * inv, v.z * inv, v.w * inv);
}

// ============================================================================
// 4D→2D 投影
// ============================================================================

ProjResult project(const Vec4 &worldPos, const Camera4D &cam,
    double scale, double offsetX, double offsetY)
{
    Vec4 v = vec4Sub(worldPos, cam.getPos());
    double camX = vec4Dot(v, cam.getRight());
    double camY = vec4Dot(v, cam.getUp());
    double camZ = vec4Dot(v, cam.getForward());
    double camW = vec4Dot(v, cam.getOver());

    // 俯仰：旋转 (camY, camZ)，视角抬头时远处物体下移
    double cp = cam.getCosPitch();
    double sp = cam.getSinPitch();
    double camYpitched = camY * cp - camZ * sp;

    constexpr double FORWARD_DEPTH_WEIGHT = 0.25;
    double depth = camW + camZ * FORWARD_DEPTH_WEIGHT;

    ProjResult result;
    result.camW = camW;

    if (depth <= 1e-6)
    {
        result.valid = false;
        result.screenPos = Vec2();
        return result;
    }

    double invDepth = 1.0 / depth;
    result.screenPos.x = camX * invDepth * scale + offsetX;
    result.screenPos.y = -camYpitched * invDepth * scale + offsetY;
    result.valid = true;
    return result;
}

// ============================================================================
// Gram-Schmidt 正交化
// ============================================================================

void gramSchmidt(Vec4 &v0, Vec4 &v1, Vec4 &v2, Vec4 &v3)
{
    // 归一化 v0
    v0 = vec4Normalize(v0);

    // v1 减去 v0 方向分量后归一化
    Vec4 u1 = vec4Sub(v1, vec4Scale(v0, vec4Dot(v1, v0)));
    v1 = vec4Normalize(u1);

    // v2 减去 v0, v1 方向分量后归一化
    Vec4 u2 = vec4Sub(v2, vec4Scale(v0, vec4Dot(v2, v0)));
    u2 = vec4Sub(u2, vec4Scale(v1, vec4Dot(u2, v1)));
    v2 = vec4Normalize(u2);

    // v3 减去 v0, v1, v2 方向分量后归一化
    Vec4 u3 = vec4Sub(v3, vec4Scale(v0, vec4Dot(v3, v0)));
    u3 = vec4Sub(u3, vec4Scale(v1, vec4Dot(u3, v1)));
    u3 = vec4Sub(u3, vec4Scale(v2, vec4Dot(u3, v2)));
    v3 = vec4Normalize(u3);
}


