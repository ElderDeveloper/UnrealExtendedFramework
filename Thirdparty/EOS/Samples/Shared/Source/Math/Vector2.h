// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// 2D vector
struct Vector2
{
    Vector2() noexcept : Vector2(0.f, 0.f) {}
    explicit Vector2(float _x) : x(_x), y(_x) {}
    Vector2(float _x, float _y) : x(_x), y(_y) {}

#ifdef DXTK
	Vector2(DirectX::XMVECTOR InVector) : x(InVector.m128_f32[0]), y(InVector.m128_f32[1]) {}
	Vector2(DirectX::XMFLOAT2 InVector) : x(InVector.x), y(InVector.y) {}
#endif


    Vector2(const Vector2&) = default;
    Vector2& operator=(const Vector2&) = default;

    Vector2(Vector2&&) = default;
    Vector2& operator=(Vector2&&) = default;

    // Comparison operators
	bool operator == (const Vector2& V) const { return x == V.x && y == V.y; }
	bool operator != (const Vector2& V) const { return x != V.x || y != V.y; }

    // Assignment operators
    Vector2& operator+= (const Vector2& V) { x += V.x; y += V.y; return *this; }
    Vector2& operator-= (const Vector2& V) { x -= V.x; y -= V.y; return *this; }
    Vector2& operator*= (float S) { x *= S; y *= S; return *this; }
    Vector2& operator/= (float S) { x /= S; y /= S; return *this; }

    // Unary operators
    Vector2 operator+ () const { return *this; }
    Vector2 operator- () const { return Vector2(-x, -y); }

	#ifdef DXTK
	operator DirectX::XMFLOAT2() const { return DirectX::XMFLOAT2(x, y); }
	#endif //DXTK
            
    // Constants
	static Vector2 Zero() { return Vector2(0.0f, 0.0f); }

	// coefficient-wise product  
	static Vector2 CoeffProduct(const Vector2& V, const Vector2& U) { return Vector2(V.x * U.x, V.y * U.y); }

	float x;
	float y;
};

// Binary operators
Vector2 operator+ (const Vector2& V1, const Vector2& V2);
Vector2 operator- (const Vector2& V1, const Vector2& V2);
Vector2 operator* (const Vector2& V1, float S);
Vector2 operator/ (const Vector2& V1, float S);