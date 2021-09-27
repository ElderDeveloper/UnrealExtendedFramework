// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedVariableLibrary.h"


FRotator UUEExtendedVariableLibrary::Add_RotatorRotator(const FRotator A, const FRotator B)
{
	return A+B;
}



FRotator UUEExtendedVariableLibrary::Divide_RotatorFloat(const FRotator A, const float B)
{
	return FRotator(A.Pitch/B,A.Yaw/B,A.Roll/B);
}



FRotator UUEExtendedVariableLibrary::Minus_RotatorRotator(const FRotator A, const FRotator B)
{
	return A-B;
}








bool UUEExtendedVariableLibrary::CompareVectorSizes(const FVector IsBigger, const FVector IsLesser)
{
	return (IsBigger.X*IsBigger.X + IsBigger.Y*IsBigger.Y + IsBigger.Z*IsBigger.Z) > (IsLesser.X*IsLesser.X + IsLesser.Y*IsLesser.Y + IsLesser.Z*IsLesser.Z);
}







FTransform UUEExtendedVariableLibrary::Add_TransformTransform(const FTransform A, const FTransform B)
{
	return FTransform( A.Rotator()+B.Rotator(),A.GetLocation()+B.GetLocation(),A.GetScale3D()+B.GetScale3D());
}








int32 UUEExtendedVariableLibrary::IncrementFloatBy(float& Float, float Value)
{
	return Float += Value; 
}



float UUEExtendedVariableLibrary::Add_Floats(const TArray<float> Array)
{
	float A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A;
}



float UUEExtendedVariableLibrary::Average_Floats(const TArray<float> Array)
{
	float A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A/2;
}







int32 UUEExtendedVariableLibrary::IncrementIntegerBy(UPARAM(ref) int32& Integer, int32 Value)
{
	return 	Integer += Value; 
}



int32 UUEExtendedVariableLibrary::Add_Integers(const TArray<int32> Array)
{
	int32 A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A;
}



int32 UUEExtendedVariableLibrary::Average_Integers(const TArray<int32> Array)
{
	int32 A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A/2;
}
