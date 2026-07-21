// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "EFDamageInterface.generated.h"


USTRUCT(BlueprintType)
struct FExtendedFrameworkDamageStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	FGameplayTag DamageTag = FGameplayTag::EmptyTag;

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	float Damage = 0;

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	FHitResult HitResult = FHitResult();

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	FVector HitNormal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	FName BoneName = FName("");

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	AActor* DamageCauser = nullptr;

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	AController* InstigatedBy = nullptr;

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	TSubclassOf<UDamageType>  DamageType = nullptr;

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	UPrimitiveComponent* HitComponent = nullptr;


	FExtendedFrameworkDamageStruct() {}

	/*
	 *	Any Damage Construct
	 */
	FExtendedFrameworkDamageStruct(FGameplayTag damageTag , float damage ,TSubclassOf<UDamageType>  damageType , AActor* damageCauser, AController* instigatedBy)
	{
		DamageTag = damageTag;
		Damage = damage;
		DamageCauser = damageCauser;
		InstigatedBy = instigatedBy;
		DamageType = damageType;
	}
	
	/*
	 *	Point Damage Construct
	 */
	FExtendedFrameworkDamageStruct(FGameplayTag damageTag , float damage, AActor* damageCauser, AController* instigatedBy , FName boneName , FVector hitLocation , FVector hitNormal , FHitResult hitResult , TSubclassOf<UDamageType>  damageType = nullptr  , UPrimitiveComponent* hitComponent = nullptr)
	{
		DamageTag = damageTag;
		Damage = damage;
		HitResult = hitResult;
		HitLocation = hitLocation;
		HitNormal = hitNormal;
		BoneName = boneName;
		DamageCauser = damageCauser;
		InstigatedBy = instigatedBy;
		DamageType = damageType;
		HitComponent = hitComponent;
	}

	/*
	 *	Area Damage Construct
	 */
	FExtendedFrameworkDamageStruct(FGameplayTag damageTag , float damage , FVector origin, AActor* damageCauser, AController* instigatedBy , FHitResult hitResult, TSubclassOf<UDamageType>  damageType = nullptr)
	{
		DamageTag = damageTag;
		Damage = damage;
		HitResult = hitResult;
		Origin = origin;
		DamageCauser = damageCauser;
		InstigatedBy = instigatedBy;
		DamageType = damageType;
	}
};



// This class does not need to be modified.
UINTERFACE()
class UEFDamageInterface : public UInterface
{
	GENERATED_BODY()
};


class UNREALEXTENDEDFRAMEWORK_API IEFDamageInterface
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended Framework Damage Interface")
	void EFApplyPointDamage(FExtendedFrameworkDamageStruct DamageStruct);

	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended Framework Damage Interface")
	void  EFRadialDamage(FExtendedFrameworkDamageStruct DamageStruct);
};
