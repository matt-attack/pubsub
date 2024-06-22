#pragma once

#include <pubsub_cpp/Vec3.h>

// Templated 4D vector
template<class T>
struct Vec4
{
	union
	{
		struct { T x, y, z, w; };
		T v[4];
	};

	Vec4() {}
	Vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
	explicit Vec4(const Vec3<T>& p, T w = 0) : x(p.x), y(p.y), z(p.z), w(w) {}
	explicit Vec4(const T* v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {}

	T &operator[](int i) { return v[i]; }
	T operator[](int i) const { return v[i]; }

	bool operator==(const Vec4& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
	bool operator!=(const Vec4& o) const { return x != o.x || y != o.y || z != o.z || w != o.w; }

	Vec4 operator+(const Vec4& o) const { return Vec4(x+o.x, y+o.y, z+o.z, w+o.w); }
	Vec4 operator-(const Vec4& o) const { return Vec4(x-o.x, y-o.y, z-o.z, w-o.w); }
	Vec4 operator+(T k) const { return Vec4(x+k, y+k, z+k, w+k); }
	Vec4 operator-(T k) const { return Vec4(x-k, y-k, z-k, w-k); }
	Vec4 operator-() const { return Vec4(-x, -y, -z, -w); }
	Vec4 neg3() const { return Vec4(-x, -y, -z, w); }
	Vec4 operator*(T k) const { return Vec4(x*k, y*k, z*k, w*k); }
	Vec4 operator/(T k) const { return Vec4(x/k, y/k, z/k, w/k); }
	Vec4 addw(T f) const { return Vec4(x, y, z, w + f); }

	Vec4 &operator+=(const Vec4& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
	Vec4 &operator-=(const Vec4& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
	Vec4 &operator+=(T k) { x += k; y += k; z += k; w += k; return *this; }
	Vec4 &operator-=(T k) { x -= k; y -= k; z -= k; w -= k; return *this; }
	Vec4 &operator*=(T k) { x *= k; y *= k; z *= k; w *= k; return *this; }
	Vec4 &operator/=(T k) { x /= k; y /= k; z /= k; w /= k; return *this; }

	T dot3(const Vec4& o) const { return x*o.x + y*o.y + z*o.z; }
	T dot3(const Vec3<T>& o) const { return x*o.x + y*o.y + z*o.z; }
	T dot(const Vec4& o) const { return dot3(o) + w*o.w; }
	T dot(const Vec3<T>& o) const { return x*o.x + y*o.y + z*o.z + w; }
	T len() const { return sqrt(dot(*this)); }
	T len3() const { return sqrt(dot3(*this)); }
	T squaredlen() const { return dot(*this); }
	T squaredlen3() const { return dot3(*this); }

	Vec4 normal() const { return *this * (1.0f / len()); }
	void normalize() { (*this) *= (1.0f/len()); }

	Vec3<T> cross3(const Vec4& o) const { return Vec3<T>(y*o.z-z*o.y, z*o.x-x*o.z, x*o.y-y*o.x); }
	Vec3<T> cross3(const Vec3<T>& o) const { return Vec3<T>(y*o.z-z*o.y, z*o.x-x*o.z, x*o.y-y*o.x); }
};

typedef Vec4<double> Vec4d;
typedef Vec4<float> Vec4f;
