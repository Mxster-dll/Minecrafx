#include "linalg.h"
#include "camera.h"   // 用于 project() 实现

// ============================================================================
// Mat4
// ============================================================================

Mat4::Mat4()
{
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m[i][j] = 0.0;
}

Mat4 Mat4::identity()
{
    Mat4 result;
    for (int i = 0; i < 4; ++i)
        result.m[i][i] = 1.0;
    return result;
}

Mat4 Mat4::fromRows(
    double r0c0, double r0c1, double r0c2, double r0c3,
    double r1c0, double r1c1, double r1c2, double r1c3,
    double r2c0, double r2c1, double r2c2, double r2c3,
    double r3c0, double r3c1, double r3c2, double r3c3)
{
    Mat4 result;
    result.m[0][0] = r0c0; result.m[0][1] = r0c1; result.m[0][2] = r0c2; result.m[0][3] = r0c3;
    result.m[1][0] = r1c0; result.m[1][1] = r1c1; result.m[1][2] = r1c2; result.m[1][3] = r1c3;
    result.m[2][0] = r2c0; result.m[2][1] = r2c1; result.m[2][2] = r2c2; result.m[2][3] = r2c3;
    result.m[3][0] = r3c0; result.m[3][1] = r3c1; result.m[3][2] = r3c2; result.m[3][3] = r3c3;
    return result;
}

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
// Mat4 运算
// ============================================================================

Mat4 mat4Mul(const Mat4 &a, const Mat4 &b)
{
    Mat4 result;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k)
                sum += a.m[i][k] * b.m[k][j];
            result.m[i][j] = sum;
        }
    }
    return result;
}

Vec4 mat4Transform(const Mat4 &m, const Vec4 &v)
{
    return Vec4(
        m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3] * v.w,
        m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3] * v.w,
        m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3] * v.w,
        m.m[3][0] * v.x + m.m[3][1] * v.y + m.m[3][2] * v.z + m.m[3][3] * v.w
    );
}

// ============================================================================
// 六种 4D 旋转矩阵
// ============================================================================

Mat4 rotateXY(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    return Mat4::fromRows(
        c, -s, 0.0, 0.0,
        s, c, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

Mat4 rotateXZ(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    return Mat4::fromRows(
        c, 0.0, -s, 0.0,
        0.0, 1.0, 0.0, 0.0,
        s, 0.0, c, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

Mat4 rotateYZ(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    return Mat4::fromRows(
        1.0, 0.0, 0.0, 0.0,
        0.0, c, -s, 0.0,
        0.0, s, c, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

Mat4 rotateXW(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    return Mat4::fromRows(
        c, 0.0, 0.0, -s,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        s, 0.0, 0.0, c
    );
}

Mat4 rotateYW(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    return Mat4::fromRows(
        1.0, 0.0, 0.0, 0.0,
        0.0, c, 0.0, -s,
        0.0, 0.0, 1.0, 0.0,
        0.0, s, 0.0, c
    );
}

Mat4 rotateZW(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    return Mat4::fromRows(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, c, -s,
        0.0, 0.0, s, c
    );
}

// ============================================================================
// 4D→2D 投影
// ============================================================================

ProjResult project(const Vec4 &worldPos, const Camera4D &cam,
    double scale, double offsetX, double offsetY, double pitch)
{
    Vec4 v = vec4Sub(worldPos, cam.getPos());
    double camX = vec4Dot(v, cam.getRight());
    double camY = vec4Dot(v, cam.getUp());
    double camZ = vec4Dot(v, cam.getForward());
    double camW = vec4Dot(v, cam.getOver());

    // 俯仰：旋转 (camY, camZ)，视角抬头时远处物体下移
    double cp = std::cos(pitch);
    double sp = std::sin(pitch);
    double camYpitched = camY * cp - camZ * sp;

    constexpr double FORWARD_DEPTH_WEIGHT = 0.25;
    double depth = camW + camZ * FORWARD_DEPTH_WEIGHT;

    ProjResult result;
    result.camW = camW;

    if (depth <= 0.1)
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

// ============================================================================
// 辅助函数
// ============================================================================

double vec4DistSq(const Vec4 &a, const Vec4 &b)
{
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    double dz = a.z - b.z;
    double dw = a.w - b.w;
    return dx * dx + dy * dy + dz * dz + dw * dw;
}
