// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EGOutlineData.generated.h"

UENUM(BlueprintType)
enum class EGOutlineType : uint8
{
	Primary,
	Secondary,
	Third
};

UENUM(BlueprintType)
enum class EGOutlineBehavior : uint8
{
	OnAllFoundPrimitiveComponents,
	OnFirstFoundPrimitiveComponent,
	OnAllFoundPrimitiveComponentsByTag,
	OnFirstFoundPrimitiveComponentByTag,
};

USTRUCT(BlueprintType)
struct FEGOutlineSymbolSettings
{
	GENERATED_BODY()

	UPROPERTY( EditAnywhere , BlueprintReadWrite , Category = "Outline Symbol Settings")
	bool bUseUpAndDownMovement;

	UPROPERTY( EditAnywhere , BlueprintReadWrite , Category = "Outline Symbol Settings")
	FVector2D UpDownRange;

	UPROPERTY( EditAnywhere , BlueprintReadWrite , Category = "Outline Symbol Settings")
	float UpAndDownSpeed;

	UPROPERTY( EditAnywhere , BlueprintReadWrite , Category = "Outline Symbol Settings")
	bool bShouldRotate;

	UPROPERTY( EditAnywhere , BlueprintReadWrite , Category = "Outline Symbol Settings")
	float RotationSpeed;

	UPROPERTY( EditAnywhere , BlueprintReadWrite , Category = "Outline Symbol Settings")
	UStaticMesh* SymbolMesh;

	UPROPERTY( EditAnywhere , BlueprintReadWrite , Category = "Outline Symbol Settings")
	FName SpawnedActorName;

	UPROPERTY( EditAnywhere , BlueprintReadWrite , Category = "Outline Symbol Settings")
	bool AttachToActor;

	UPROPERTY( EditAnywhere , BlueprintReadWrite , Category = "Outline Symbol Settings")
	AActor* ParentActor;

	FEGOutlineSymbolSettings()
	{
		bUseUpAndDownMovement = false;
		UpDownRange = FVector2d(10 , 50);;
		UpAndDownSpeed = 1.0f;
		bShouldRotate = false;
		RotationSpeed = 1.0f;
		SymbolMesh = nullptr;
		SpawnedActorName = FName("OutlineSymbol");
		AttachToActor = false;
		ParentActor = nullptr;
	}
};

UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGOutlineData : public UObject
{
	GENERATED_BODY()
};
