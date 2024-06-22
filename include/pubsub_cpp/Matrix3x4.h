#pragma once

#include <pubsub_cpp/Vec4.h>
#include <pubsub_cpp/Quaternion.h>
#include <pubsub_cpp/Matrix3x3.h>

// Matrix3x4 class used for representing rotations, translations and scales
template<class T>
struct Matrix3x4
{
  Vec4<T> a, b, c;

  Matrix3x4() {}
  Matrix3x4(const Vec4<T>& a, const Vec4<T>& b, const Vec4<T>& c) : a(a), b(b), c(c) {}

  explicit Matrix3x4(const Matrix3x3<T>& rot, const Vec3<T>& trans)
    : a(Vec4<T>(rot.a, trans.x)), b(Vec4<T>(rot.b, trans.y)), c(Vec4<T>(rot.c, trans.z)) {}

  explicit Matrix3x4(const Quaternion& rot, const Vec3<T>& trans)
  {
    *this = Matrix3x4(Matrix3x3<T>(rot), trans);
  }

  explicit Matrix3x4(const Quaternion& rot, const Vec3<T>& trans, const Vec3<T>& scale)
  {
    *this = Matrix3x4(Matrix3x3<T>(rot, scale), trans);
  }

  explicit Matrix3x4(const Vec3<T>& trans)
    : a(Vec4<T>(1,0,0,trans.x)), b(Vec4<T>(0,1,0,trans.y)), c(Vec4<T>(0,0,1,trans.z)) {}

  static Matrix3x4 Identity()
  {
    return Matrix3x4(Vec4<T>(1,0,0,0), Vec4<T>(0,1,0,0), Vec4<T>(0,0,1,0));	
  }

  Matrix3x4 operator*(T k) const { return Matrix3x4(*this) *= k; }
  Matrix3x4& operator*=(T k)
  {
    a *= k;
    b *= k;
    c *= k;
    return *this;
  }

  Matrix3x4 operator+(const Matrix3x4& o) const { return Matrix3x4(*this) += o; }
  Matrix3x4& operator+=(const Matrix3x4& o)
  {
    a += o.a;
    b += o.b;
    c += o.c;
    return *this;
  }

  Matrix3x4 operator*(const Matrix3x4& o) const
  {
    return Matrix3x4(
      (a*o.a.x + b*o.a.y + c*o.a.z).addw(o.a.w),
      (a*o.b.x + b*o.b.y + c*o.b.z).addw(o.b.w),
      (a*o.c.x + b*o.c.y + c*o.c.z).addw(o.c.w));
  }
  Matrix3x4 &operator*=(const Matrix3x4& o) { return (*this = *this * o); }

  // Sets the matrix equal to the inverse of itself
  void invert()
  {
    Matrix3x3<T> invrot(Vec3<T>(a.x, b.x, c.x), Vec3<T>(a.y, b.y, c.y), Vec3<T>(a.z, b.z, c.z));
    invrot.a /= invrot.a.squaredlen();
    invrot.b /= invrot.b.squaredlen();
    invrot.c /= invrot.c.squaredlen();
    Vec3<T> trans(a.w, b.w, c.w);
    a = Vec4<T>(invrot.a, -invrot.a.dot(trans));
    b = Vec4<T>(invrot.b, -invrot.b.dot(trans));
    c = Vec4<T>(invrot.c, -invrot.c.dot(trans));
  }

  // Transforms the given vector by the matrix
  Vec3<T> transform(const Vec3<T>& o) const { return Vec3<T>(a.dot(o), b.dot(o), c.dot(o)); }
  Vec3<T> transformnormal(const Vec3<T>& o) const { return Vec3<T>(a.dot3(o), b.dot3(o), c.dot3(o)); }
};

typedef Matrix3x4<float> Matrix3x4f;
typedef Matrix3x4<double> Matrix3x4d;
