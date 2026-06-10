#include "camera.h"

Camera4D::Camera4D()
    : m_pos(0.0, 2.0, 0.0, -5.0)   // w 偏移，使 y=0,w=0 平面的地板可见
    , m_right(1.0, 0.0, 0.0, 0.0)
    , m_up(0.0, 1.0, 0.0, 0.0)
    , m_forward(0.0, 0.0, 1.0, 0.0)
    , m_over(0.0, 0.0, 0.0, 1.0)
{
    // 初始倾斜 over 向量，使射线检测能命中地板
    rotateYW(-0.4);
}

void Camera4D::move(const Vec4 &direction)
{
    m_pos = vec4Add(m_pos, direction);
}

// ---- 六种平面旋转 ----
// 每个旋转作用于两个基向量，使用 2D 旋转公式，其余基向量不变

void Camera4D::rotateXY(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    // right 和 up 在 XY 平面旋转（X=right, Y=up）
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
    // right 和 forward 在 XZ 平面旋转（X=right, Z=forward）
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
    // up 和 forward 在 YZ 平面旋转
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
    // right 和 over 在 XW 平面旋转
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
    // up 和 over 在 YW 平面旋转
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
    // forward 和 over 在 ZW 平面旋转
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

void Camera4D::orthonormalize()
{
    gramSchmidt(m_right, m_up, m_forward, m_over);
}

void Camera4D::reset()
{
    m_pos = Vec4(0.0, 2.0, 0.0, -5.0);
    m_right = Vec4(1.0, 0.0, 0.0, 0.0);
    m_up = Vec4(0.0, 1.0, 0.0, 0.0);
    m_forward = Vec4(0.0, 0.0, 1.0, 0.0);
    m_over = Vec4(0.0, 0.0, 0.0, 1.0);
    rotateYW(-0.4);  // 恢复初始倾斜
}
