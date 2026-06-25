#include "camera.h"
#include "project4d.h"

// NOTE 正交化太频繁，有浮点漂移风险

Camera4D::Camera4D()
    : m_pos(12.0, 20.0, -2.0, 6.0)
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

void Camera4D::rotateAroundUp(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);

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

void Camera4D::rotateSlice(double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);

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

    m_up = Vec4(0.0, 1.0, 0.0, 0.0);
    m_right.y = 0.0;
    m_forward.y = 0.0;
    m_over.y = 0.0;
    gramSchmidt(m_right, m_forward, m_over, m_up);
}

Plane2D Camera4D::getViewPlane() const
{

    Vec3 n = Vec3::fromVec4(m_over);

    Vec3 pRef = Vec3::fromVec4(m_right);

    double offset = n.x * m_pos.x + n.z * m_pos.z + n.w * m_pos.w;

    return Plane2D::fromNormal(n, pRef, offset);
}