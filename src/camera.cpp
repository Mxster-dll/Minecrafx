#include "camera.h"
#include "project4d.h"

Camera4D::Camera4D()
    : m_pos(1.5, 2.0, -5.0, 0.5)
    , m_right(1.0, 0.0, 0.0, 0.0)
    , m_up(0.0, 1.0, 0.0, 0.0)
    , m_forward(0.0, 0.0, 1.0, 0.0)
    , m_over(0.0, 0.0, 0.0, 1.0)
    , m_pitch(0.0)
    , m_cosPitch(1.0)
    , m_sinPitch(0.0)
{}

void Camera4D::move(const Vec4 &direction)
{
    m_pos = vec4Add(m_pos, direction);
}

// ---- 六种平面旋转 ----
// 仅 rotateXZ / rotateXW / rotateZW 不触及 up=(0,1,0,0)
// rotateXY / rotateYZ / rotateYW 会破坏高度轴锁定，保留但不推荐使用

void Camera4D::rotateXY(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    Vec4 newRight(
        m_right.x * c - m_up.x * s,
        m_right.y * c - m_up.y * s,
        m_right.z * c - m_up.z * s,
        m_right.w * c - m_up.w * s
    );
    Vec4 newUp(
        m_right.x * s + m_up.x * c,
        m_right.y * s + m_up.y * c,
        m_right.z * s + m_up.z * c,
        m_right.w * s + m_up.w * c
    );
    m_right = newRight;
    m_up = newUp;
    orthonormalize();
}

void Camera4D::rotateXZ(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    // right 和 forward 在 XZ 平面旋转（不涉及 Y）
    Vec4 newRight(
        m_right.x * c - m_forward.x * s,
        m_right.y * c - m_forward.y * s,
        m_right.z * c - m_forward.z * s,
        m_right.w * c - m_forward.w * s
    );
    Vec4 newForward(
        m_right.x * s + m_forward.x * c,
        m_right.y * s + m_forward.y * c,
        m_right.z * s + m_forward.z * c,
        m_right.w * s + m_forward.w * c
    );
    m_right = newRight;
    m_forward = newForward;
    orthonormalize();
}

void Camera4D::rotateYZ(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    Vec4 newUp(
        m_up.x * c - m_forward.x * s,
        m_up.y * c - m_forward.y * s,
        m_up.z * c - m_forward.z * s,
        m_up.w * c - m_forward.w * s
    );
    Vec4 newForward(
        m_up.x * s + m_forward.x * c,
        m_up.y * s + m_forward.y * c,
        m_up.z * s + m_forward.z * c,
        m_up.w * s + m_forward.w * c
    );
    m_up = newUp;
    m_forward = newForward;
    orthonormalize();
}

void Camera4D::rotateXW(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    // right 和 over 在 XW 平面旋转（不涉及 Y）
    Vec4 newRight(
        m_right.x * c - m_over.x * s,
        m_right.y * c - m_over.y * s,
        m_right.z * c - m_over.z * s,
        m_right.w * c - m_over.w * s
    );
    Vec4 newOver(
        m_right.x * s + m_over.x * c,
        m_right.y * s + m_over.y * c,
        m_right.z * s + m_over.z * c,
        m_right.w * s + m_over.w * c
    );
    m_right = newRight;
    m_over = newOver;
    orthonormalize();
}

void Camera4D::rotateYW(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    Vec4 newUp(
        m_up.x * c - m_over.x * s,
        m_up.y * c - m_over.y * s,
        m_up.z * c - m_over.z * s,
        m_up.w * c - m_over.w * s
    );
    Vec4 newOver(
        m_up.x * s + m_over.x * c,
        m_up.y * s + m_over.y * c,
        m_up.z * s + m_over.z * c,
        m_up.w * s + m_over.w * c
    );
    m_up = newUp;
    m_over = newOver;
    orthonormalize();
}

void Camera4D::rotateZW(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    // forward 和 over 在 ZW 平面旋转（不涉及 Y）
    Vec4 newForward(
        m_forward.x * c - m_over.x * s,
        m_forward.y * c - m_over.y * s,
        m_forward.z * c - m_over.z * s,
        m_forward.w * c - m_over.w * s
    );
    Vec4 newOver(
        m_forward.x * s + m_over.x * c,
        m_forward.y * s + m_over.y * c,
        m_forward.z * s + m_over.z * c,
        m_forward.w * s + m_over.w * c
    );
    m_forward = newForward;
    m_over = newOver;
    orthonormalize();
}

void Camera4D::rotateAroundUp(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    // 在 {right, forward} 平面内绕 up 旋转（偏航）
    Vec4 newRight(
        m_right.x * c - m_forward.x * s,
        m_right.y * c - m_forward.y * s,
        m_right.z * c - m_forward.z * s,
        m_right.w * c - m_forward.w * s);
    Vec4 newForward(
        m_right.x * s + m_forward.x * c,
        m_right.y * s + m_forward.y * c,
        m_right.z * s + m_forward.z * c,
        m_right.w * s + m_forward.w * c);
    m_right = newRight;
    m_forward = newForward;
    orthonormalize();
}

void Camera4D::rotateAroundRight(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    // 在 {forward, up} 平面内绕 right 旋转（俯仰）
    Vec4 newForward(
        m_forward.x * c + m_up.x * s,
        m_forward.y * c + m_up.y * s,
        m_forward.z * c + m_up.z * s,
        m_forward.w * c + m_up.w * s);
    Vec4 newUp(
        m_up.x * c - m_forward.x * s,
        m_up.y * c - m_forward.y * s,
        m_up.z * c - m_forward.z * s,
        m_up.w * c - m_forward.w * s);
    m_forward = newForward;
    m_up = newUp;
    orthonormalize();
}

void Camera4D::rotateAroundForward(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    // 在 {up, over} 平面内绕 forward 旋转（滚转 / 切片角度）
    Vec4 newUp(
        m_up.x * c - m_over.x * s,
        m_up.y * c - m_over.y * s,
        m_up.z * c - m_over.z * s,
        m_up.w * c - m_over.w * s);
    Vec4 newOver(
        m_up.x * s + m_over.x * c,
        m_up.y * s + m_over.y * c,
        m_up.z * s + m_over.z * c,
        m_up.w * s + m_over.w * c);
    m_up = newUp;
    m_over = newOver;
    orthonormalize();
}

void Camera4D::rotateSlice(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    // 绕 right=j 轴旋转 forward↔over（切片平面 span{right,forward} 绕 j 在 XZW 内转）
    // right 和 up 保持不变
    Vec4 newForward(
        m_forward.x * c - m_over.x * s,
        m_forward.y * c - m_over.y * s,
        m_forward.z * c - m_over.z * s,
        m_forward.w * c - m_over.w * s);
    Vec4 newOver(
        m_forward.x * s + m_over.x * c,
        m_forward.y * s + m_over.y * c,
        m_forward.z * s + m_over.z * c,
        m_forward.w * s + m_over.w * c);
    m_forward = newForward;
    m_over = newOver;
    orthonormalize();
}

void Camera4D::setPitch(double angle)
{
    m_pitch = angle;
    m_cosPitch = std::cos(m_pitch);
    m_sinPitch = std::sin(m_pitch);
}

void Camera4D::addPitch(double delta)
{
    static const double halfPi = 1.5707963267948966;

    m_pitch += delta;

    if (m_pitch > halfPi) m_pitch = halfPi;
    if (m_pitch < -halfPi) m_pitch = -halfPi;

    m_cosPitch = std::cos(m_pitch);
    m_sinPitch = std::sin(m_pitch);
}

void Camera4D::orthonormalize()
{
    // 锁定 up = 全局高度轴 (0,1,0,0)
    m_up = Vec4(0.0, 1.0, 0.0, 0.0);
    m_right.y = 0.0;
    m_forward.y = 0.0;
    m_over.y = 0.0;
    gramSchmidt(m_right, m_forward, m_over, m_up);
}

Plane2D Camera4D::getViewPlane() const
{
    // 法向量 n = over 在 xzw 子空间中的分量
    Vec3 n = Vec3::fromVec4(m_over);

    // 参考基向量 p = right 在 xzw 子空间中的分量
    // 正交化后 right ⊥ over 且 right 无 y 分量，故 p 已在平面上
    Vec3 pRef = Vec3::fromVec4(m_right);

    // 偏移量 offset = n · camPos_xzw（使平面穿过摄像机位置）
    double offset = n.x * m_pos.x + n.z * m_pos.z + n.w * m_pos.w;

    return Plane2D::fromNormal(n, pRef, offset);
}

void Camera4D::reset()
{
    m_pos = Vec4(1.5, 2.0, -5.0, 0.5);
    m_right = Vec4(1.0, 0.0, 0.0, 0.0);
    m_up = Vec4(0.0, 1.0, 0.0, 0.0);
    m_forward = Vec4(0.0, 0.0, 1.0, 0.0);
    m_over = Vec4(0.0, 0.0, 0.0, 1.0);
    m_pitch = 0.0;
    m_cosPitch = 1.0;
    m_sinPitch = 0.0;
}
