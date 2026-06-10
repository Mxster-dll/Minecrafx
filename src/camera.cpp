#include "camera.h"

Camera4D::Camera4D()
    : m_pos(0.0, 1.0, 0.0, -5.0)   // y=1 站在地板上方，w=-5 使地板可见
    , m_right(1.0, 0.0, 0.0, 0.0)
    , m_up(0.0, 1.0, 0.0, 0.0)
    , m_forward(0.0, 0.0, 1.0, 0.0)
    , m_over(0.0, 0.0, 0.0, 1.0)
{
    // 预旋转：使 right/forward/over 在 XZW 子空间内充分混合
    // 这样 WASD 移动会同时影响 X、Z、W 三轴（仅 Y 轴由 Space/Shift 独占）
    rotateXZ(0.6);
    rotateXW(0.4);
    rotateZW(0.8);
}

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

void Camera4D::orthonormalize()
{
    // 先锁定 up = (0,1,0,0)（高度轴永不偏离）
    m_up = Vec4(0.0, 1.0, 0.0, 0.0);

    // 清除 right/forward/over 中可能因浮点误差积累的 Y 分量
    m_right.y = 0.0;
    m_forward.y = 0.0;
    m_over.y = 0.0;

    // Gram-Schmidt 正交化（right → forward → over，up 已固定）
    gramSchmidt(m_right, m_forward, m_over, m_up);
    // 注意：gramSchmidt 最后处理 m_up，但 m_up 已归一化且与其他向量正交
    //       由于 right/forward/over 的 y=0，m_up=(0,1,0,0) 不会被修改
}

void Camera4D::reset()
{
    m_pos = Vec4(0.0, 1.0, 0.0, -5.0);
    m_right = Vec4(1.0, 0.0, 0.0, 0.0);
    m_up = Vec4(0.0, 1.0, 0.0, 0.0);
    m_forward = Vec4(0.0, 0.0, 1.0, 0.0);
    m_over = Vec4(0.0, 0.0, 0.0, 1.0);    // 恢复初始混合旋转
    rotateXZ(0.6);
    rotateXW(0.4);
    rotateZW(0.8);
}
