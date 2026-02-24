// Fill out your copyright notice in the Description page of Project Settings.


#include "EFConversionLibrary.h"
#include "Kismet/KismetStringLibrary.h"


// ================================ STRING ================================

FVector UEFConversionLibrary::FStringToVector(const FString& in)
{
	FVector Out;
	bool Success;
	UKismetStringLibrary::Conv_StringToVector(in, Out, Success);
	if (!Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFConversionLibrary::FStringToVector : Failed to parse '%s' as FVector"), *in);
	}
	return Out;
}

FRotator UEFConversionLibrary::FStringToRotator(const FString& in)
{
	FRotator Out;
	bool Success;
	UKismetStringLibrary::Conv_StringToRotator(in, Out, Success);
	if (!Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFConversionLibrary::FStringToRotator : Failed to parse '%s' as FRotator"), *in);
	}
	return Out;
}

FQuat UEFConversionLibrary::FStringToQuat(const FString& in)
{
	FRotator Out;
	bool Success;
	UKismetStringLibrary::Conv_StringToRotator(in, Out, Success);
	if (!Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFConversionLibrary::FStringToQuat : Failed to parse '%s' as FQuat (via Rotator)"), *in);
		return FQuat::Identity;
	}
	// Use the proper Rotator->Quaternion conversion which produces a normalized quaternion
	return Out.Quaternion();
}


// ================================ FLOAT ================================

int32 UEFConversionLibrary::FloatToInt32(float in, EIntContype Type)
{
	switch (Type)
	{
		case Direct:    return static_cast<int32>(in);
		case Floor:     return FMath::FloorToInt(in);
		case Ceil:      return FMath::CeilToInt(in);
		case Round:     return FMath::RoundToInt(in);
		case Truncate:  return FMath::TruncToInt(in);
		default:        return static_cast<int32>(in);
	}
}

int64 UEFConversionLibrary::FloatToInt64(float in, EIntContype Type)
{
	switch (Type)
	{
		case Direct:    return static_cast<int64>(in);
		case Floor:     return static_cast<int64>(FMath::FloorToInt(in));
		case Ceil:      return static_cast<int64>(FMath::CeilToInt(in));
		case Round:     return static_cast<int64>(FMath::RoundToInt(in));
		case Truncate:  return static_cast<int64>(FMath::TruncToInt(in));
		default:        return static_cast<int64>(in);
	}
}

FVector UEFConversionLibrary::FloatToVector(float in, int32 XYZ)
{
	switch (XYZ)
	{
	case 0:    return FVector(in, in, in);
	case 1:    return FVector(in, 0, 0);
	case 2:    return FVector(0, in, 0);
	case 3:    return FVector(0, 0, in);
	case 4:    return FVector(in, in, 0);
	case 5:    return FVector(in, 0, in);
	case 6:    return FVector(0, in, in);
	default:   return FVector(in, in, in);
	}
}

FRotator UEFConversionLibrary::FloatToRotator(float in, int32 XYZ)
{
	switch (XYZ)
	{
	case 0:    return FRotator(in, in, in);
	case 1:    return FRotator(in, 0, 0);
	case 2:    return FRotator(0, in, 0);
	case 3:    return FRotator(0, 0, in);
	case 4:    return FRotator(in, in, 0);
	case 5:    return FRotator(in, 0, in);
	case 6:    return FRotator(0, in, in);
	default:   return FRotator(in, in, in);
	}
}

FQuat UEFConversionLibrary::FloatToQuat(float in, float inW, int32 XYZ)
{
	FQuat Out;
	switch (XYZ)
	{
	case 0:    Out = FQuat(in, in, in, inW);   break;
	case 1:    Out = FQuat(in, 0, 0, inW);     break;
	case 2:    Out = FQuat(0, in, 0, inW);     break;
	case 3:    Out = FQuat(0, 0, in, inW);     break;
	case 4:    Out = FQuat(in, in, 0, inW);    break;
	case 5:    Out = FQuat(in, 0, in, inW);    break;
	case 6:    Out = FQuat(0, in, in, inW);    break;
	default:   Out = FQuat(in, in, in, inW);   break;
	}
	// BUG FIX: Normalize the quaternion to prevent unexpected rotation behavior
	// from non-unit quaternions
	Out.Normalize();
	return Out;
}


// ================================ INT32 ================================

FVector UEFConversionLibrary::Int32ToVector(const int32 in, const int32 XYZ)
{
	switch (XYZ)
	{
	case 0:    return FVector(in, in, in);
	case 1:    return FVector(in, 0, 0);
	case 2:    return FVector(0, in, 0);
	case 3:    return FVector(0, 0, in);
	case 4:    return FVector(in, in, 0);
	case 5:    return FVector(in, 0, in);
	case 6:    return FVector(0, in, in);
	default:   return FVector(in, in, in);
	}
}

FRotator UEFConversionLibrary::Int32ToRotator(const int32 in, const int32 XYZ)
{
	switch (XYZ)
	{
	case 0:    return FRotator(in, in, in);
	case 1:    return FRotator(in, 0, 0);
	case 2:    return FRotator(0, in, 0);
	case 3:    return FRotator(0, 0, in);
	case 4:    return FRotator(in, in, 0);
	case 5:    return FRotator(in, 0, in);
	case 6:    return FRotator(0, in, in);
	default:   return FRotator(in, in, in);
	}
}

FQuat UEFConversionLibrary::Int32ToQuat(const int32 in, const float inW, const int32 XYZ)
{
	FQuat Out;
	switch (XYZ)
	{
	case 0:    Out = FQuat(in, in, in, inW);   break;
	case 1:    Out = FQuat(in, 0, 0, inW);     break;
	case 2:    Out = FQuat(0, in, 0, inW);     break;
	case 3:    Out = FQuat(0, 0, in, inW);     break;
	case 4:    Out = FQuat(in, in, 0, inW);    break;
	case 5:    Out = FQuat(in, 0, in, inW);    break;
	case 6:    Out = FQuat(0, in, in, inW);    break;
	default:   Out = FQuat(in, in, in, inW);   break;
	}
	Out.Normalize();
	return Out;
}


// ================================ COLOR ================================

FLinearColor UEFConversionLibrary::StringToColor(const FString& in)
{
	FLinearColor Result = FLinearColor::Black;

	// Try hex format first (e.g. "#FF0000FF" or "FF0000FF")
	FString HexString = in;
	if (HexString.StartsWith(TEXT("#")))
	{
		HexString = HexString.RightChop(1);
	}

	if (HexString.Len() == 6 || HexString.Len() == 8)
	{
		Result = FLinearColor(FColor::FromHex(in));
		return Result;
	}

	// Try "R=... G=... B=... A=..." format
	Result.InitFromString(in);
	return Result;
}

FString UEFConversionLibrary::ColorToString(const FLinearColor& in)
{
	return in.ToFColor(true).ToHex();
}


// ================================ VECTOR/ROTATOR TO QUAT ================================

FQuat UEFConversionLibrary::VectorToQuat(const FVector& in, const float inW)
{
	FQuat Out(in.Y, in.Z, in.X, inW);
	Out.Normalize();
	return Out;
}

FQuat UEFConversionLibrary::RotatorToQuat(const FRotator& in, const float inW)
{
	FQuat Out(in.Pitch, in.Yaw, in.Roll, inW);
	Out.Normalize();
	return Out;
}
