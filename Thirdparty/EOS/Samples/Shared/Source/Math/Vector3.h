// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// 3D vector
struct Vector3
{
    Vector3() noexcept : Vector3(0.f, 0.f, 0.f) {}
    explicit Vector3(float _x) : x(_x), y(_x), z(_x) {}
	explicit Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}


    Vector3(const Vector3&) = default;
    Vector3& operator=(const Vector3&) = default;

    Vector3(Vector3&&) = default;
    Vector3& operator=(Vector3&&) = default;

    // Comparison operators
	bool operator == (const Vector3& V) const { return x == V.x && y == V.y && z == V.z; }
	bool operator != (const Vector3& V) const { return x != V.x || y != V.y || z != V.z; }

    // Assignment operators
	Vector3& operator+= (const Vector3& V) { x += V.x; y += V.y; z += V.z; return *this; }
	Vector3& operator-= (const Vector3& V) { x -= V.x; y -= V.y; z -= V.z; return *this; }
	Vector3& operator*= (float S) { x *= S; y *= S; z *= S; return *this; }
	Vector3& operator/= (float S) { x /= S; y /= S; z /= S; return *this; }

    // Unary operators
    Vector3 operator+ () const { return *this; }
    Vector3 operator- () const { return Vector3(-x, -y, -z); }

	Vector3 Cross(const Vector3& V);
	void Normalize();

	#ifdef DXTK
	operator DirectX::XMFLOAT3() const { return DirectX::XMFLOAT3(x, y, z); }
	operator DirectX::SimpleMath::Vector3() const { return DirectX::SimpleMath::Vector3(x, y, z); }
	#endif //DXTK

	static Vector3 Zero() { return Vector3(0.0f); }
			
	float x;
	float y;
	float z;
};

// Binary operators
Vector3 operator+ (const Vector3& V1, const Vector3& V2);
Vector3 operator- (const Vector3& V1, const Vector3& V2);
Vector3 operator* (const Vector3& V1, float S);
Vector3 operator/ (const Vector3& V1, float S);