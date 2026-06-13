#pragma once

#include <cmath>

// ============================================================================
// 前向声明
// ============================================================================
class Camera4D;

// ============================================================================
// Vec2 — 2D 屏幕坐标
// ============================================================================
struct Vec2
{
    double x, y;

    Vec2() : x(0.0), y(0.0) {}
    Vec2(double x, double y) : x(x), y(y) {}
};

// ============================================================================
// Vec4 — 四维向量
// ============================================================================
struct Vec4
{
    double x, y, z, w;

    Vec4() : x(0.0), y(0.0), z(0.0), w(0.0) {}
    Vec4(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {}
};

// ============================================================================
// 4×4 矩阵
// ============================================================================
struct Mat4
{
    double m[4][4];

    /** @brief 构造零矩阵 */
    Mat4();

    /** @brief 构造单位矩阵 */
    static Mat4 identity();

    /** @brief 从 16 个值构造（行优先） */
    static Mat4 fromRows(
        double r0c0, double r0c1, double r0c2, double r0c3,
        double r1c0, double r1c1, double r1c2, double r1c3,
        double r2c0, double r2c1, double r2c2, double r2c3,
        double r3c0, double r3c1, double r3c2, double r3c3);
};

// ============================================================================
// 投影结果
// ============================================================================
struct ProjResult
{
    bool valid;      ///< 投影是否有效（w > 0.1 不裁剪）
    Vec2 screenPos;  ///< 屏幕坐标
    double camW;     ///< 摄像机空间 w 分量（用于深度排序/颜色）
};

// ============================================================================
// Vec4 向量运算（小驼峰命名）
// ============================================================================

/** @brief 向量加法 */
Vec4 vec4Add(const Vec4 &a, const Vec4 &b);

/** @brief 向量减法 */
Vec4 vec4Sub(const Vec4 &a, const Vec4 &b);

/** @brief 标量乘法 */
Vec4 vec4Scale(const Vec4 &v, double s);

/** @brief 点积 */
double vec4Dot(const Vec4 &a, const Vec4 &b);

/** @brief 向量长度 */
double vec4Length(const Vec4 &v);

/** @brief 向量长度平方（避免 sqrt） */
double vec4LengthSq(const Vec4 &v);

/** @brief 归一化，零向量返回零向量 */
Vec4 vec4Normalize(const Vec4 &v);

// ============================================================================
// Mat4 矩阵运算
// ============================================================================

/** @brief 矩阵乘法 C = A * B */
Mat4 mat4Mul(const Mat4 &a, const Mat4 &b);

/** @brief 矩阵变换向量 v' = M * v */
Vec4 mat4Transform(const Mat4 &m, const Vec4 &v);

// ============================================================================
// 六种 4D 旋转矩阵生成函数
// ============================================================================

/** @brief 绕 XY 平面旋转（Z/W 轴不动） */
Mat4 rotateXY(double angle);

/** @brief 绕 XZ 平面旋转（Y/W 轴不动） */
Mat4 rotateXZ(double angle);

/** @brief 绕 YZ 平面旋转（X/W 轴不动） */
Mat4 rotateYZ(double angle);

/** @brief 绕 XW 平面旋转（Y/Z 轴不动） */
Mat4 rotateXW(double angle);

/** @brief 绕 YW 平面旋转（X/Z 轴不动） */
Mat4 rotateYW(double angle);

/** @brief 绕 ZW 平面旋转（X/Y 轴不动） */
Mat4 rotateZW(double angle);

// ============================================================================
// 4D→2D 投影
// ============================================================================

/**
 * @brief 将世界坐标投影到屏幕坐标
 * @param worldPos  世界空间中的 4D 点
 * @param cam       摄像机引用
 * @param scale     投影缩放因子
 * @param offsetX   屏幕 X 偏移（通常为半宽）
 * @param offsetY   屏幕 Y 偏移（通常为半高）
 * @return ProjResult，包含有效性、屏幕坐标和 camW 深度
 */
ProjResult project(const Vec4 &worldPos, const Camera4D &cam,
    double scale, double offsetX, double offsetY);

// ============================================================================
// Gram-Schmidt 正交化
// ============================================================================

/**
 * @brief 对四个基向量执行 Gram-Schmidt 正交归一化
 * @param v0  第一个基向量（就地修改）
 * @param v1  第二个基向量（就地修改）
 * @param v2  第三个基向量（就地修改）
 * @param v3  第四个基向量（就地修改）
 *
 * 处理顺序：v0 归一化 → v1 减去 v0 分量后归一化 →
 *          v2 减去 v0,v1 分量后归一化 → v3 减去 v0,v1,v2 分量后归一化
 * 若某向量归一化后为零向量，则保持原值不变以避免崩溃。
 */
void gramSchmidt(Vec4 &v0, Vec4 &v1, Vec4 &v2, Vec4 &v3);

// ============================================================================
// 辅助函数
// ============================================================================

/** @brief 计算两个 Vec4 之间的平方距离 */
double vec4DistSq(const Vec4 &a, const Vec4 &b);
