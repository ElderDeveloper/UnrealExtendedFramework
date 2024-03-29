﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "EFVariableLibrary.h"


#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"


FRotator UEFVariableLibrary::Add_RotatorRotator(const FRotator A, const FRotator B)
{
	return A+B;
}



FRotator UEFVariableLibrary::Divide_RotatorFloat(const FRotator A, const float B)
{
	return FRotator(A.Pitch/B,A.Yaw/B,A.Roll/B);
}



FRotator UEFVariableLibrary::Minus_RotatorRotator(const FRotator A, const FRotator B)
{
	return A-B;
}




bool UEFVariableLibrary::CompareVectorSizes(const FVector IsBigger, const FVector IsLesser)
{
	return (IsBigger.X*IsBigger.X + IsBigger.Y*IsBigger.Y + IsBigger.Z*IsBigger.Z) > (IsLesser.X*IsLesser.X + IsLesser.Y*IsLesser.Y + IsLesser.Z*IsLesser.Z);
}

FVector UEFVariableLibrary::Vector_NormalizeScaled(const FVector Vector, float Tolerance, float Scale)
{
	return Vector.GetSafeNormal(Tolerance)*Scale;
}

void UEFVariableLibrary::VectorGetUpDownValues(const FVector& VectorRef, float UpValue, float DownValue, FVector& UpVector,FVector& DownVector)
{
	UpVector = VectorRef + FVector(0,0,UpValue);
	DownVector = VectorRef + FVector(0,0,DownValue);
}


FTransform UEFVariableLibrary::Add_TransformTransform(const FTransform A, const FTransform B)
{
	return FTransform( A.Rotator()+B.Rotator(),A.GetLocation()+B.GetLocation(),A.GetScale3D()+B.GetScale3D());
}








float UEFVariableLibrary::IncrementFloatBy(float& Float, float Value)
{
	return Float += Value; 
}



float UEFVariableLibrary::Add_Floats(const TArray<float> Array)
{
	float A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A;
}



float UEFVariableLibrary::Average_Floats(const TArray<float> Array)
{
	float A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A/2;
}

float UEFVariableLibrary::Divide_FloatInt(float Float, int32 divider)
{
	return Float/divider;
}

float UEFVariableLibrary::Divide_FloatByte(float Float, uint8 divider)
{
	return Float/divider;
}


int32 UEFVariableLibrary::IncrementIntegerBy(UPARAM(ref) int32& Integer, int32 Value)
{
	return 	Integer += Value; 
}



int32 UEFVariableLibrary::Add_Integers(const TArray<int32> Array)
{
	int32 A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A;
}



int32 UEFVariableLibrary::Average_Integers(const TArray<int32> Array)
{
	int32 A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A/2;
}

FString UEFVariableLibrary::GetFIntPointAsString(const FIntPoint& Point, const FString Separator)
{
	return FString::FromInt(Point.X) + Separator + FString::FromInt(Point.Y);
}

FString UEFVariableLibrary::GetScreenResolutionAsString(UGameUserSettings*& UserSettings, const FString Separator)
{
	if (const auto GameUserSettings = GEngine->GameUserSettings)
	{
		const auto Resolution = GameUserSettings->GetScreenResolution();
		UserSettings = GameUserSettings;
		return FString::FromInt(Resolution.X) + Separator + FString::FromInt(Resolution.Y); 
	}
	return "";
}



