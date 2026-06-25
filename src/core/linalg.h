#pragma once

#include <cmath>

class Camera4D;

struct Vec2
{
    double x, y;

    Vec2() : x(0.0), y(0.0) {}
    Vec2(double x, double y) : x(x), y(y) {}
};

struct Vec4
{
    double x, y, z, w;

    Vec4() : x(0.0), y(0.0), z(0.0), w(0.0) {}
    Vec4(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {}
};

struct ProjResult
{
    bool valid;
    Vec2 screenPos;
    double camW;
};

Vec4 vec4Add(const Vec4 &a, const Vec4 &b);

Vec4 vec4Sub(const Vec4 &a, const Vec4 &b);

Vec4 vec4Scale(const Vec4 &v, double s);

double vec4Dot(const Vec4 &a, const Vec4 &b);

double vec4Length(const Vec4 &v);

double vec4LengthSq(const Vec4 &v);

Vec4 vec4Normalize(const Vec4 &v);

ProjResult project(const Vec4 &worldPos, const Camera4D &cam,
    double scale, double offsetX, double offsetY);

void gramSchmidt(Vec4 &v0, Vec4 &v1, Vec4 &v2, Vec4 &v3);