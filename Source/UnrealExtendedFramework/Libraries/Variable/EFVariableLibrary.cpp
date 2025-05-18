// Fill out your copyright notice in the Description page of Project Settings.


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


bool UEFVariableLibrary::CompareVectorSizes(const FVector& IsBigger, const FVector& IsLesser)
{
	return (IsBigger.X*IsBigger.X + IsBigger.Y*IsBigger.Y + IsBigger.Z*IsBigger.Z) > (IsLesser.X*IsLesser.X + IsLesser.Y*IsLesser.Y + IsLesser.Z*IsLesser.Z);
}


FVector UEFVariableLibrary::Vector_NormalizeScaled(const FVector& Vector, float Tolerance, float Scale)
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


float UEFVariableLibrary::Average_Floats(const TArray<float>& Array)
{
	float A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A/2;
}


int32 UEFVariableLibrary::Average_Integers(const TArray<int32>& Array)
{
	int32 A = 0;
	for (const auto i : Array)
	{
		A +=i;
	}
	return A/2;
}


FString UEFVariableLibrary::GetFIntPointAsString(const FIntPoint& Point, const FString& Separator)
{
	return FString::FromInt(Point.X) + Separator + FString::FromInt(Point.Y);
}


FString UEFVariableLibrary::GetScreenResolutionAsString(UGameUserSettings*& UserSettings, const FString& Separator)
{
	if (const auto GameUserSettings = GEngine->GameUserSettings)
	{
		const auto Resolution = GameUserSettings->GetScreenResolution();
		UserSettings = GameUserSettings;
		return FString::FromInt(Resolution.X) + Separator + FString::FromInt(Resolution.Y); 
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
	FString Password;
	const FString Numbers = TEXT("0123456789");
	const FString Symbols = TEXT("!@#$%^&*()_+-=[]{}|;:,.<>?/");
	const FString Uppercase = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	const FString Lowercase = TEXT("abcdefghijklmnopqrstuvwxyz");

	if (bIncludeNumbers)
	{
		Password += Numbers;
	}
	if (bIncludeSymbols)
	{
		Password += Symbols;
	}
	if (bIncludeUppercase)
	{
		Password += Uppercase;
	}
	if (bIncludeLowercase)
	{
		Password += Lowercase;
	}

	FString RandomPassword;
	for (int32 i = 0; i < Length; ++i)
	{
		RandomPassword += Password[FMath::RandRange(0, Length - 1)];
	}

	return RandomPassword;
}
