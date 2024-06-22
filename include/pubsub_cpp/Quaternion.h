#pragma once

#include <pubsub_cpp/Matrix3x4.h>

// Quaternion for representing rotations
struct Quaternion
{
  float w, x, y, z;

  inline Quaternion() : w(1), x(0), y(0), z(0) {}
  inline Quaternion(float W, float X, float Y, float Z) : w(W), x(X), y(Y), z(Z) {}

  // Builds a quaternion representing a rotation around an axis in radians
  static Quaternion FromAngleAxis(float angle, const Vec3f& axis)
  {
    float half_angle = 0.5*angle;
    float vsin = sin(half_angle);
    return Quaternion(cos(half_angle), vsin*axis.x, vsin*axis.y, vsin*axis.z);
  }

  // Builds a quaternion representing a rotation around an axis in radians
  static Quaternion FromAngleAxis(float angle, const Vec3d& axis)
  {
    return FromAngleAxis(angle, Vec3f(axis.x, axis.y, axis.z));
  }

  // Converts a quaternion to a rotation around an axis in radians
  void ToAngleAxis(float& angle, Vec3f& axis) const
  {
    // q = cos(angle/2)+sin(angle/2)*(x*i+y*j+z*k)
    float sqr_len = x*x + y*y + z*z;
    if (sqr_len > 0.0)
    {
      angle = 2.0*acos(w);
      float inv_len = 1.0f/sqrt(sqr_len);
      axis.x = x*inv_len;
      axis.y = y*inv_len;
      axis.z = z*inv_len;
    }
    else
    {
      // angle is 0 so pick any axis
      angle = 0.0;
      axis.x = 1.0;
      axis.y = 0.0;
      axis.z = 0.0;
    }
  }

  inline Quaternion& operator=(const Quaternion& rhs)
  {
    w = rhs.w;
    x = rhs.x;
    y = rhs.y;
    z = rhs.z;
    return *this;
  }

  Quaternion operator*(const Quaternion& rhs) const
  {
    return Quaternion
    (
      w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
      w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
      w * rhs.y + y * rhs.w + z * rhs.x - x * rhs.z,
      w * rhs.z + z * rhs.w + x * rhs.y - y * rhs.x
    );
  }

  Quaternion operator*(float f) const
  {
    return Quaternion(w * f, x * f, y * f, z * f);
  }

  inline bool operator==(const Quaternion& rhs) const
  {
    return (rhs.x == x) && (rhs.y == y) &&
      (rhs.z == z) && (rhs.w == w);
  }

  inline bool operator!=(const Quaternion& rhs) const
  {
    return !operator==(rhs);
  }

  // Turns this into a unit quaternion
  Quaternion normalized() const { return *this * (1.0f / sqrt(norm())); }

  // Returns the dot product of the quaternions
  float dot(const Quaternion& o) const
  {
    return w*o.w + x*o.x + y*o.y + z*o.z;
  }
  
  // Returns the "length" of the quaternion
  float norm() const
  {
    return w*w + x*x + y*y + z*z;
  }

  // Normalises this quaternion, and returns the previous length
  float normalize()
  {
    float len = norm();
    float factor = 1.0f/sqrt(len);
    *this = *this * factor;
    return len;
  }

  // Returns the inverse of the given quaternion
  Quaternion inverse() const
  {
    float fNorm = w*w + x*x + y*y + z*z;
    if (fNorm > 0.0)
    {
      float fInvNorm = 1.0f/fNorm;
      return Quaternion(w*fInvNorm, -x*fInvNorm, -y*fInvNorm, -z*fInvNorm);
    }
    else
    {
      // return an invalid result to flag the error
      return Quaternion(0, 0, 0, 0);
    }
  }

  // Rotates the supplied vector
  Vec3f operator*(const Vec3f& v) const
  {
    Vec3f qvec(x, y, z);
    Vec3f uv = qvec.cross(v);
    Vec3f uuv = qvec.cross(uv);
    uv *= (2.0f*w);
    uuv *= 2.0f;

    return v + uv + uuv;
  }

  // Rotates the supplied vector
  Vec3d operator*(const Vec3d& v) const
  {
    Vec3d qvec(x, y, z);
    Vec3d uv = qvec.cross(v);
    Vec3d uuv = qvec.cross(uv);
    uv *= (2.0f*w);
    uuv *= 2.0f;

    return v + uv + uuv;
  }
};
