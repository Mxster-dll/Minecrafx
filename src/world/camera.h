#pragma once

#include "../core/linalg.h"
#include "project4d.h"

class Camera4D
{
public:

    Camera4D();

    void move(const Vec4 &direction);

    void rotateAroundUp(double angle);

    void rotateSlice(double angle);

    void setPitch(double angle);

    void addPitch(double delta);

    double getPitch() const { return m_pitch; }

    double getCosPitch() const { return m_cosPitch; }
    double getSinPitch() const { return m_sinPitch; }

    void orthonormalize();

    const Vec4 &getPos()     const { return m_pos; }
    const Vec4 &getRight()   const { return m_right; }
    const Vec4 &getUp()      const { return m_up; }
    const Vec4 &getForward() const { return m_forward; }
    const Vec4 &getOver()    const { return m_over; }

    Plane2D getViewPlane() const;

private:
    Vec4 m_pos;
    Vec4 m_right;
    Vec4 m_up;
    Vec4 m_forward;
    Vec4 m_over;
    double m_pitch;
    mutable double m_cosPitch;
    mutable double m_sinPitch;
};