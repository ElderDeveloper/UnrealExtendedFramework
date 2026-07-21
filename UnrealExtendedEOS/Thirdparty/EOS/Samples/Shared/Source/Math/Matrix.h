// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef DXTK
#include "SimpleMath.h"

using FMatrix = DirectX::SimpleMath::Matrix;
#endif

#ifdef EOS_DEMO_SDL

struct FMatrix
{
	float data[4][4];

	inline FMatrix operator*(const FMatrix& Mat) const;
};

inline FMatrix FMatrix::operator*(const FMatrix& Mat) const
{
	FMatrix Result;
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			Result.data[i][j] = data[i][0] * Mat.data[0][j] +
				data[i][1] * Mat.data[1][j] +
				data[i][2] * Mat.data[2][j] +
				data[i][3] * Mat.data[3][j];
		}
	}
	return Result;
}

inline FMatrix BuildIdentity()
{
	FMatrix Result;
	for (size_t i = 0; i < 16; ++i)
		Result.data[i / 4][i % 4] = ((i / 4) == (i % 4)) ? 1.0f : 0.0f;

	return Result;
}

inline FMatrix BuildTranslate(float X, float Y, float Z)
{
	FMatrix Result = BuildIdentity();
	Result.data[3][0] = X;
	Result.data[3][1] = Y;
	Result.data[3][2] = Z;

	return Result;
}

inline FMatrix BuildScale(float X, float Y, float Z)
{
	FMatrix Result = BuildIdentity();
	Result.data[0][0] = X;
	Result.data[1][1] = Y;
	Result.data[2][2] = Z;

	return Result;
}

inline FMatrix BuildRotation(float Angle, Vector3 Axis)
{
	const float Degree2Rad = 0.0174533f;
	float Rad = Angle * Degree2Rad;
	float C = cos(Rad);
	float S = sin(Rad);
	float T = (1.0f - C);
	
	Axis.Normalize();

	FMatrix Result;
	Result.data[0][0] = C + Axis.x * Axis.x * T;
	Result.data[1][0] = Axis.y * Axis.x * T + Axis.z * S;
	Result.data[2][0] = Axis.z * Axis.x * T - Axis.y * S;
	Result.data[3][0] = 0.0f;
	Result.data[0][1] = Axis.x * Axis.y * T - Axis.z * S;
	Result.data[1][1] = C + Axis.y * Axis.y * T;
	Result.data[2][1] = Axis.z * Axis.y * T + Axis.x * S;
	Result.data[3][1] = 0.0f;
	Result.data[0][2] = Axis.x * Axis.z * T + Axis.y * S;
	Result.data[1][2] = Axis.y * Axis.z * T - Axis.x * S;
	Result.data[2][2] = Axis.z * Axis.z * T + C;
	Result.data[3][2] = 0.0f;
	Result.data[0][3] = 0.0f;
	Result.data[1][3] = 0.0f;
	Result.data[2][3] = 0.0f;
	Result.data[3][3] = 1.0f;

	return Result;
}

inline FMatrix BuildOrtho(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
 	FMatrix Result = BuildIdentity();
 	Result.data[0][0] = 2.0f / (Right - Left);
 	Result.data[1][1] = 2.0f / (Top - Bottom);
 	Result.data[2][2] = -2.0f / (Far - Near);
 	Result.data[3][0] = -(Right + Left) / (Right - Left);
 	Result.data[3][1] = -(Top + Bottom) / (Top - Bottom);
 	Result.data[3][2] = -(Far + Near) / (Far - Near);

	return Result;
}

inline FMatrix BuildPerspProjMat(float FOV, float Aspect, float ZNear, float ZFar)
{
	FMatrix Result;

	const float Degree2Rad = 0.0174533f;

	float f = 1 / tan(Degree2Rad * FOV / 2.0f);

	Result.data[0][0] = f / Aspect;
	Result.data[0][1] = 0;
	Result.data[0][2] = 0;
	Result.data[0][3] = 0;

	Result.data[1][0] = 0;
	Result.data[1][1] = f;
	Result.data[1][2] = 0;
	Result.data[1][3] = 0;

	Result.data[2][0] = 0;
	Result.data[2][1] = 0;
	Result.data[2][2] = (ZFar + ZNear) / (ZNear - ZFar);
	Result.data[2][3] = -1;

	Result.data[3][0] = 0;
	Result.data[3][1] = 0;
	Result.data[3][2] = 2.0f * ZFar * ZNear / (ZNear - ZFar);
	Result.data[3][3] = 0;

	return Result;
}

inline FMatrix BuildLookAtMatrix(float EyeX, float EyeY, float EyeZ,
	float CenterX, float CenterY, float CenterZ,
	float UpX, float UpY, float UpZ)
{
	FMatrix Result;
	Vector3 Side, Up, Forward;

	//Forward
	Forward.x = CenterX - EyeX;
	Forward.y = CenterY - EyeY;
	Forward.z = CenterZ - EyeZ;

	Forward.Normalize();

	// Up
	Up.x = UpX;
	Up.y = UpY;
	Up.z = UpZ;
	Up.Normalize();

	Side = Forward.Cross(Up);
	Side.Normalize();

	Up = Side.Cross(Forward);
	Up.Normalize();

	Result.data[0][0] = Side.x;
	Result.data[1][0] = Side.y;
	Result.data[2][0] = Side.z;
	Result.data[3][0] = 0.0;
	Result.data[0][1] = Up.x;
	Result.data[1][1] = Up.y;
	Result.data[2][1] = Up.z;
	Result.data[3][1] = 0.0;
	Result.data[0][2] = -Forward.x;
	Result.data[1][2] = -Forward.y;
	Result.data[2][2] = -Forward.z;
	Result.data[3][2] = 0.0;
	Result.data[0][3] = 0.0;
	Result.data[1][3] = 0.0;
	Result.data[2][3] = 0.0;
	Result.data[3][3] = 1.0;

	FMatrix Translate = BuildTranslate(-EyeX, -EyeY, -EyeZ);
	return Translate * Result;
}

#endif //EOS_DEMO_SDL

