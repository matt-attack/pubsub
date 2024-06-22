#pragma once

#include <cmath>

// Templated 3D vector
template<class T>
struct Vec3
{
  union
  {
    struct { T x, y, z; };
    T v[3];
  };

  Vec3() {}
  Vec3(T x, T y, T z) : x(x), y(y), z(z) {};
  explicit Vec3(const T* v) : x(v[0]), y(v[1]), z(v[2]) {};

  inline T &operator[](int i) { return v[i]; }
  inline T operator[](int i) const { return v[i]; }

  bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
  bool operator!=(const Vec3& o) const { return x != o.x || y != o.y || z != o.z; }

  Vec3 operator+(const Vec3 &o) const { return Vec3(x+o.x, y+o.y, z+o.z); }
  Vec3 operator-(const Vec3 &o) const { return Vec3(x-o.x, y-o.y, z-o.z); }
  Vec3 operator+(T k) const { return Vec3(x+k, y+k, z+k); }
  Vec3 operator-(T k) const { return Vec3(x-k, y-k, z-k); }
  Vec3 operator-() const { return Vec3(-x, -y, -z); }
  Vec3 operator*(const Vec3& o) const { return Vec3(x*o.x, y*o.y, z*o.z); }
  Vec3 operator/(const Vec3& o) const { return Vec3(x/o.x, y/o.y, z/o.z); }
  Vec3 operator*(T k) const { return Vec3(x*k, y*k, z*k); }
  Vec3 operator/(T k) const { return Vec3(x/k, y/k, z/k); }

  Vec3 &operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  Vec3 &operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  Vec3 &operator+=(T k) { x += k; y += k; z += k; return *this; }
  Vec3 &operator-=(T k) { x -= k; y -= k; z -= k; return *this; }
  Vec3 &operator*=(const Vec3& o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
  Vec3 &operator/=(const Vec3& o) { x /= o.x; y /= o.y; z /= o.z; return *this; }
  Vec3 &operator*=(T k) { x *= k; y *= k; z *= k; return *this; }
  Vec3 &operator/=(T k) { x /= k; y /= k; z /= k; return *this; }

  T dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
  T len() const { return sqrt(dot(*this)); }
  T squaredlen() const { return dot(*this); }
  T dist(const Vec3& o) const { return (*this - o).len(); }
  T distsqr(const Vec3& o) const { return (*this - o).squaredlen(); }

  Vec3 normal() const { return *this * (1.0f / len()); }
  void normalize() { (*this) *= (1.0f / len()); }

  Vec3 cross(const Vec3& o) const { return Vec3(y*o.z-z*o.y, z*o.x-x*o.z, x*o.y-y*o.x); }
};

typedef Vec3<double> Vec3d;
typedef Vec3<float> Vec3f;
