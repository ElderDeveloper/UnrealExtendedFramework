// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Vector2.h"

// Binary operators
Vector2 operator+ (const Vector2& V1, const Vector2& V2)
{
	return Vector2(V1.x + V2.x, V1.y + V2.y);
}
Vector2 operator- (const Vector2& V1, const Vector2& V2)
{
	return Vector2(V1.x - V2.x, V1.y - V2.y);
}

Vector2 operator* (const Vector2& V1, float S)
{
	return Vector2(V1.x * S, V1.y * S);
}

Vector2 operator/ (const Vector2& V1, float S)
{
	return Vector2(V1.x / S, V1.y / S);
}
