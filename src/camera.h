#pragma once

#include "linalg.h"

/**
 * @brief 4D 摄像机类
 *
 * 维护位置和四个正交基向量（right, up, forward, over），
 * 提供六种 4D 旋转平面的旋转和 Gram-Schmidt 正交化。
 */
class Camera4D
{
public:
    /** @brief 构造默认摄像机：位置 (0,1,0,-5)，up 锁定为 (0,1,0,0) */
    Camera4D();

    // ---- 移动 ----

    /** @brief 沿给定方向移动摄像机位置 */
    void move(const Vec4 &direction);

    // ---- 六种平面旋转 ----

    /** @brief 绕 XY 平面旋转（影响 right 和 up） */
    void rotateXY(double angle);
    /** @brief 绕 XZ 平面旋转（影响 right 和 forward） */
    void rotateXZ(double angle);
    /** @brief 绕 YZ 平面旋转（影响 up 和 forward） */
    void rotateYZ(double angle);
    /** @brief 绕 XW 平面旋转（影响 right 和 over） */
    void rotateXW(double angle);
    /** @brief 绕 YW 平面旋转（影响 up 和 over） */
    void rotateYW(double angle);
    /** @brief 绕 ZW 平面旋转（影响 forward 和 over） */
    void rotateZW(double angle);

    // ---- 正交化 ----

    /** @brief 对四个基向量执行 Gram-Schmidt 正交归一化 */
    void orthonormalize();

    // ---- 重置 ----

    /** @brief 重置摄像机到初始状态 */
    void reset();

    // ---- Getters ----

    const Vec4 &getPos()     const { return m_pos; }
    const Vec4 &getRight()   const { return m_right; }
    const Vec4 &getUp()      const { return m_up; }
    const Vec4 &getForward() const { return m_forward; }
    const Vec4 &getOver()    const { return m_over; }

private:
    Vec4 m_pos;
    Vec4 m_right;
    Vec4 m_up;
    Vec4 m_forward;
    Vec4 m_over;
};
