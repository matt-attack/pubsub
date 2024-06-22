#pragma once

#include <pubsub_cpp/Vec4.h>
#include <pubsub_cpp/Quaternion.h>

// Matrix3x3 class generally used for representing rotations and scales
template<class T>
struct Matrix3x3
{
  Vec3<T> a, b, c;

  Matrix3x3() {}
  Matrix3x3(const Vec3<T>& a, const Vec3<T>& b, const Vec3<T>& c) : a(a), b(b), c(c) {}

  explicit Matrix3x3(const Quaternion& q)
  {
    convertquat(q, *this);
  }

  explicit Matrix3x3(const Quaternion& q, const Vec3<T>& scale)
  {
    convertquat(q, *this);
    a *= scale;
    b *= scale;
    c *= scale;
  }

  // Converts a Quaternion to a Matrix3x3
  inline static void convertquat(const Quaternion& q, Matrix3x3& mat)
  {
    T x = q.x, y = q.y, z = q.z, w = q.w,
    tx = 2*x, ty = 2*y, tz = 2*z,
    txx = tx*x, tyy = ty*y, tzz = tz*z,
    txy = tx*y, txz = tx*z, tyz = ty*z,
    twx = w*tx, twy = w*ty, twz = w*tz;
    mat.a = Vec3<T>(1 - (tyy + tzz), txy - twz, txz + twy);
    mat.b = Vec3<T>(txy + twz, 1 - (txx + tzz), tyz - twx);
    mat.c = Vec3<T>(txz - twy, tyz + twx, 1 - (txx + tyy));
  }

  Matrix3x3 operator*(const Matrix3x3& o) const
  {
    return Matrix3x3(
      o.a*a.x + o.b*a.y + o.c*a.z,
      o.a*b.x + o.b*b.y + o.c*b.z,
      o.a*c.x + o.b*c.y + o.c*c.z);
  }
  Matrix3x3& operator*=(const Matrix3x3& o) { return (*this = *this * o); }

  // Sets the matrix equal to the transpose of itself
  void transpose()
  {
    Vec3<T> at(a.x, b.x, c.x);
    Vec3<T> bt(a.y, b.y, c.y);
    Vec3<T> ct(a.z, b.z, c.z);
    a = at;
    b = bt;
    c = ct;
  }

  // Transforms the given vector by the matrix
  Vec3<T> transform(const Vec3<T>& o) const { return Vec3<T>(a.dot(o), b.dot(o), c.dot(o)); }
  Vec3<T> transposedtransform(const Vec3<T>& o) const { return a*o.x + b*o.y + c*o.z; }
};

typedef Matrix3x3<float> Matrix3x3f;
typedef Matrix3x3<double> Matrix3x3d;
