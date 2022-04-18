// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFMonitorLibrary.generated.h"

USTRUCT(BlueprintType)
struct FRect
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Rectangle")
	float Left = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Rectangle")
	float Top= 0;
	UPROPERTY(BlueprintReadWrite, Category = "Rectangle")
	float Right= 0;
	UPROPERTY(BlueprintReadWrite, Category = "Rectangle")
	float Bottom= 0;
};

USTRUCT(BlueprintType)
struct FDisplayInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FString Name = "";
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FString ID = "";
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	int32 NativeWidth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	int32 NativeHeight = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FIntPoint MaxResolution = FIntPoint(ForceInitToZero);
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FRect DisplayRect = FRect();
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FRect WorkArea = FRect();
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	bool bIsPrimary = false;
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	int32 DPI = 0;
};

UENUM(BlueprintType)
enum class EMonitorAspectRatio : uint8
{
	Aspect4_3		UMETA(DisplayName = "4:3"),
	Aspect5_4		UMETA(DisplayName = "5:4"),
	Aspect16_9		UMETA(DisplayName = "16:9"),
	Aspect16_10		UMETA(DisplayName = "16:10"),
	Custom			UMETA(DisplayName = "Custom"),
};

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFMonitorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Monitor Info", Keywords = "Display Device Info Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "DisplayInfo") FDisplayInfo GetMonitorInfo(int32 Index = 0);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Number Attached Monitors", Keywords = "Display Device Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "Number") int32 GetNAttachedMonitors();

	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Monitor Aspect Ratio", Keywords = "Display Device Native Aspect Ratio Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "AspectRatio") EMonitorAspectRatio GetMonitorAspectRatio(int32 Index, float& Ratio, bool& IsWide);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Primary Monitor Index", Keywords = "Display Device Primary Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "Index") int32 GetPrimaryMonitorIndex();

	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Max Monitor Resolution", Keywords = "Display Device Resolution Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "MaxResolution") FIntPoint GetMaxMonitorResolution(int32 Index = 0);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Monitor Safe Areas", Keywords = "Display Device Safe Monitor"), Category = "Display Info")
	static void GetMonitorSafeAreas(FRect& TitleSafeArea, float& TitleSafeRatio, FRect& ActionSafeArea);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Primary Display Resolution", Keywords = "Display Device Primary Resolution Monitor"), Category = "Display Info")
	static void GetPrimaryDisplayResolution(int32& Width, int32& Height);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Print Display Info To Log", Keywords = "Display Device Info Log Print Monitor"), Category = "Display Info")
	static void PrintDisplayInfoToLog();

	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Resolution Aspect Ratio", Keywords = "Display Device Resolution Aspect Ratio"), Category = "Display Info")
	static UPARAM(DisplayName = "AspectRatio") EMonitorAspectRatio GetResolutionAspectRatio(FIntPoint Resolution, float& Ratio, bool& IsWide);
	
};
