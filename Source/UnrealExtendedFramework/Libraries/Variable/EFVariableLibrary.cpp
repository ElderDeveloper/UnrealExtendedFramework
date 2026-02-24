// Fill out your copyright notice in the Description page of Project Settings.


#include "EFVariableLibrary.h"


#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"


FRotator UEFVariableLibrary::Add_RotatorRotator(const FRotator A, const FRotator B)
{
	return A + B;
}


FRotator UEFVariableLibrary::Divide_RotatorFloat(const FRotator A, const float B)
{
	// BUG FIX: Added division-by-zero guard
	if (FMath::IsNearlyZero(B))
	{
		return FRotator::ZeroRotator;
	}
	return FRotator(A.Pitch / B, A.Yaw / B, A.Roll / B);
}


FRotator UEFVariableLibrary::Minus_RotatorRotator(const FRotator A, const FRotator B)
{
	return A - B;
}


bool UEFVariableLibrary::CompareVectorSizes(const FVector& IsBigger, const FVector& IsLesser)
{
	return IsBigger.SizeSquared() > IsLesser.SizeSquared();
}


FVector UEFVariableLibrary::Vector_NormalizeScaled(const FVector& Vector, float Tolerance, float Scale)
{
	return Vector.GetSafeNormal(Tolerance) * Scale;
}


void UEFVariableLibrary::VectorGetUpDownValues(const FVector& VectorRef, float UpValue, float DownValue, FVector& UpVector, FVector& DownVector)
{
	UpVector = VectorRef + FVector(0, 0, UpValue);
	DownVector = VectorRef + FVector(0, 0, DownValue);
}


FTransform UEFVariableLibrary::Add_TransformTransform(const FTransform A, const FTransform B)
{
	return FTransform(A.Rotator() + B.Rotator(), A.GetLocation() + B.GetLocation(), A.GetScale3D() + B.GetScale3D());
}


float UEFVariableLibrary::Average_Floats(const TArray<float>& Array)
{
	// BUG FIX: Previously divided by 2 instead of Array.Num().
	// Now correctly computes the arithmetic average.
	if (Array.Num() == 0)
	{
		return 0.f;
	}

	float Sum = 0.f;
	for (const auto i : Array)
	{
		Sum += i;
	}
	return Sum / Array.Num();
}


int32 UEFVariableLibrary::Average_Integers(const TArray<int32>& Array)
{
	// BUG FIX: Previously divided by 2 instead of Array.Num().
	if (Array.Num() == 0)
	{
		return 0;
	}

	int32 Sum = 0;
	for (const auto i : Array)
	{
		Sum += i;
	}
	return Sum / Array.Num();
}


float UEFVariableLibrary::WeightedAverage(const TArray<float>& Values, const TArray<float>& Weights)
{
	if (Values.Num() == 0 || Values.Num() != Weights.Num())
	{
		return 0.f;
	}

	float WeightedSum = 0.f;
	float TotalWeight = 0.f;
	for (int32 i = 0; i < Values.Num(); ++i)
	{
		WeightedSum += Values[i] * Weights[i];
		TotalWeight += Weights[i];
	}

	if (FMath::IsNearlyZero(TotalWeight))
	{
		return 0.f;
	}

	return WeightedSum / TotalWeight;
}


FString UEFVariableLibrary::GetFIntPointAsString(const FIntPoint& Point, const FString& Separator)
{
	return FString::FromInt(Point.X) + Separator + FString::FromInt(Point.Y);
}


FString UEFVariableLibrary::GetScreenResolutionAsString(UGameUserSettings*& UserSettings, const FString& Separator)
{
	if (GEngine)
	{
		if (const auto GameUserSettings = GEngine->GameUserSettings)
		{
			const auto Resolution = GameUserSettings->GetScreenResolution();
			UserSettings = GameUserSettings;
			return FString::FromInt(Resolution.X) + Separator + FString::FromInt(Resolution.Y); 
		}
	}
	return "";
}


FGameplayTag UEFVariableLibrary::GetGameplayTagFromName(const FName& Name)
{
	if (Name.IsNone())
	{
		return FGameplayTag::EmptyTag;
	}
	
	FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Name);
	if (Tag.IsValid())
	{
		return Tag;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("GetGameplayTagFromFName() Invalid tag: %s"), *Name.ToString());
	return FGameplayTag::EmptyTag;
}


FGameplayTag UEFVariableLibrary::GetGameplayTagFromString(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return FGameplayTag::EmptyTag;
	}
	
	FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Name));
	if (Tag.IsValid())
	{
		return Tag;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("GetGameplayTagFromString() Invalid tag: %s"), *Name);
	return FGameplayTag::EmptyTag;
}


FGameplayTag UEFVariableLibrary::GetGameplayTagFromText(const FText& Name)
{
	if (Name.IsEmpty())
	{
		return FGameplayTag::EmptyTag;
	}
	
	FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Name.ToString()));
	if (Tag.IsValid())
	{
		return Tag;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("GetGameplayTagFromText() Invalid tag: %s"), *Name.ToString());
	return FGameplayTag::EmptyTag;
}

FName UEFVariableLibrary::GetNameFromText(const FText& Name)
{
	return FName(*Name.ToString());
}



FString UEFVariableLibrary::GetRandomPassword(int32 Length, bool bIncludeNumbers, bool bIncludeSymbols, bool bIncludeUppercase, bool bIncludeLowercase)
{
	FString Pool;
	const FString Numbers = TEXT("0123456789");
	const FString Symbols = TEXT("!@#$%^&*()_+-=[]{}|;:,.<>?/");
	const FString Uppercase = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	const FString Lowercase = TEXT("abcdefghijklmnopqrstuvwxyz");

	if (bIncludeNumbers)
	{
		Pool += Numbers;
	}
	if (bIncludeSymbols)
	{
		Pool += Symbols;
	}
	if (bIncludeUppercase)
	{
		Pool += Uppercase;
	}
	if (bIncludeLowercase)
	{
		Pool += Lowercase;
	}

	// BUG FIX: Previously used Length - 1 as max index into Pool.
	// If Length > Pool.Len(), this would access out of bounds and crash.
	// Now correctly uses Pool.Len() - 1 as the max index.
	if (Pool.Len() == 0 || Length <= 0)
	{
		return FString();
	}

	FString RandomPassword;
	RandomPassword.Reserve(Length);
	for (int32 i = 0; i < Length; ++i)
	{
		RandomPassword += Pool[FMath::RandRange(0, Pool.Len() - 1)];
	}

	return RandomPassword;
}
