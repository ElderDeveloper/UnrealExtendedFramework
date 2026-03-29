// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Vector3.h"

// Binary operators
Vector3 operator+ (const Vector3& V1, const Vector3& V2)
{
	return Vector3(V1.x + V2.x, V1.y + V2.y, V1.z + V2.z);
}
Vector3 operator- (const Vector3& V1, const Vector3& V2)
{
	return Vector3(V1.x - V2.x, V1.y - V2.y, V1.z - V2.z);
}

Vector3 operator* (const Vector3& V1, float S)
{
	return Vector3(V1.x * S, V1.y * S, V1.z * S);
}

Vector3 operator/ (const Vector3& V1, float S)
{
	return Vector3(V1.x / S, V1.y / S, V1.z / S);
}

Vector3 Vector3::Cross(const Vector3& V)
{
	return Vector3(y * V.z - z * V.y,  -(x * V.z - z * V.x), x * V.y - y * V.x);
}

void Vector3::Normalize()
{
	float Mag = sqrt(x * x + y * y + z * z);
	if (Mag)
	{
		x /= Mag;
		y /= Mag;
		z /= Mag;
	}
}
