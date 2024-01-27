// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/EFAttributeData.h"
#include "EFAttributeComponent.generated.h"


class UEFAttributeObject;
class AController;
class UDamageType;
class AActor;
class UDataTable;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeModified ,const FExtendedAttribute& , Attribute);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnTakeDamage , AActor* , DamagedActor , float , Damage , const class UDamageType* , DamageType , class AController* , InstigatedBy , AActor* , DamageCauser , FHitResult , HitInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActorDeath , AActor* , DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeedAttributeModified, float , Speed);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) , Blueprintable , BlueprintType)
class UNREALEXTENDEDFRAMEWORK_API UEFAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	
	UEFAttributeComponent();
	
	UPROPERTY(BlueprintAssignable , Category= "Attribute")
	FOnAttributeModified OnAttributeModified;

	UFUNCTION()
	void OnAttributeModifiedTrigger(const FExtendedAttribute& Attribute);

	UPROPERTY(BlueprintAssignable , Category= "Attribute")
	FOnTakeDamage OnTakeDamage;

	UPROPERTY(BlueprintAssignable , Category= "Attribute")
	FOnActorDeath OnActorDeath;
	
	UPROPERTY(Transient)
	FOnActorDeath OnDeath;
	
	UPROPERTY(BlueprintAssignable , Category= "Attribute")
	FOnSpeedAttributeModified OnSpeedAttributeModified;

	UPROPERTY(EditDefaultsOnly , Category= "Attribute")
	TArray<FGameplayTag> DamageImmunityTags;

	UFUNCTION(BlueprintNativeEvent , Category= "Attribute")
	void OnOwnerDeath( AActor* DamagedActor , float Damage , const class UDamageType* DamageType , class AController* InstigatedBy , AActor* DamageCauser);

	UFUNCTION( BlueprintCallable , Category= "Attribute")
	float AddAttributeValue(const FGameplayTag Attribute , const float Value = 0);

	UFUNCTION( BlueprintCallable , Category= "Attribute")
	float AddExtendedAttribute(FExtendedAttribute Attribute);

	UFUNCTION( BlueprintCallable , Category= "Attribute")
	bool RemoveExtendedAttribute(FExtendedAttribute Attribute);
	
	UFUNCTION(BlueprintCallable , Category= "Attribute")
	bool RemoveAttributeValue(const FGameplayTag Attribute , const float Value = 0);
	
	UFUNCTION(BlueprintCallable , Category= "Attribute")
	bool RemoveAttribute(const FGameplayTag AttributeTag);

	
protected:

	UPROPERTY(EditDefaultsOnly , Category="Attribute")
	FGameplayTag HealthAttribute;

	UPROPERTY(EditDefaultsOnly , Category="Attribute")
	FGameplayTag MovementSpeedAttribute;

	// This is the table that contains all the attributes that will be added to the character on BeginPlay , It only accepts "ExtendedAttribute" as a row structure
	UPROPERTY(EditAnywhere ,meta=(RequiredAssetDataTags = "RowStructure=/Script/UnrealExtendedFramework.ExtendedAttribute") , Category= "Attribute")
	UDataTable* StartupAttributeTable;

	UPROPERTY(BlueprintReadWrite, Category= "Attribute")
	TArray<FExtendedAttributeDependency> AttributeDependencies;

	UPROPERTY(BlueprintReadWrite, Category= "Attribute")
	FExtendedAttribute LastModifiedAttribute;

	UPROPERTY(BlueprintReadWrite, Category= "Attribute")
	TArray<FExtendedAttribute> BaseAttributes;
	
	UPROPERTY(BlueprintReadWrite, Category= "Attribute")
	TArray<FExtendedAttribute> EffectAttributes;
	
	UPROPERTY(BlueprintReadWrite, Category= "Attribute")
	TArray<FExtendedAttribute> CalculatedAttributes;

	UPROPERTY(BlueprintReadWrite, Category= "Attribute")
	bool bIsAlive = true;

	UPROPERTY(BlueprintReadWrite, Category= "Attribute")
	TMap<FGameplayTag , UEFAttributeObject*> AttributeObjects;

	UPROPERTY(BlueprintReadWrite, Category= "Attribute")
	bool bBlockDamage = false;

	UFUNCTION(BlueprintCallable , Category= "Attribute")
	void BlockDamageAny();

	UFUNCTION(BlueprintCallable , Category= "Attribute")
	void InitializeStartupAttributes();

	UFUNCTION(BlueprintCallable , Category= "Attribute")
	void CalculateAttributes();

	UFUNCTION(BlueprintCallable , Category= "Attribute")
	void UpdateAndBroadcastLastModifiedAttribute(const FExtendedAttribute& Attribute);

	UFUNCTION(BlueprintCallable , Category= "Attribute")
	void AddAttribute(TArray<FExtendedAttribute>& TargetAttributes , const FExtendedAttribute& SourceAttribute);

	UFUNCTION(BlueprintCallable , Category= "Attribute")
	void AddAttributes(TArray<FExtendedAttribute>& TargetAttributes , const TArray<FExtendedAttribute>& SourceAttributes);
	
	UFUNCTION(BlueprintCallable , Category= "Attribute")
	void ReceiveDamage( AActor* DamagedActor , float Damage , const class UDamageType* DamageType , class AController* InstigatedBy , AActor* DamageCauser , FHitResult HitInfo);
	
	void UpdateSpeedAttribute(const FExtendedAttribute& Attribute);
	
	UFUNCTION()
	void OwnerTakeAnyDamage(AActor* DamagedActor , float Damage , const class UDamageType* DamageType , class AController* InstigatedBy , AActor* DamageCauser);

	UFUNCTION()
	void OwnerTakePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser );

	UFUNCTION()
	void OwnerTakeRadialDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, FVector Origin, const FHitResult& HitInfo, AController* InstigatedBy,AActor* DamageCauser);

	virtual void BeginPlay() override;

public:

	UFUNCTION(BlueprintPure , Category= "Attribute")
	float GetBaseAttributeValue(const FGameplayTag AttributeTag) const;

	UFUNCTION(BlueprintPure , Category= "Attribute")
	float GetEffectAttributeValue(const FGameplayTag AttributeTag) const;

	UFUNCTION(BlueprintPure , Category= "Attribute")
	FExtendedAttribute GetAttribute(const FGameplayTag AttributeTag) const;

	UFUNCTION(BlueprintPure , Category= "Attribute")
	float GetAttributeValue(const FGameplayTag AttributeTag) const;

	UFUNCTION(BlueprintPure , Category= "Attribute")
	void GetTwoAttributeValue(const FGameplayTag AttributeOne ,const FGameplayTag AttributeTwo , float& ValueOne , float& ValueTwo) const;

	UFUNCTION(BlueprintPure , Category= "Attribute")
	float GetTwoAttributePercent(const FGameplayTag AttributeOne ,const FGameplayTag AttributeTwo) const;

	UFUNCTION(BlueprintPure , Category= "Attribute")
	bool GetHasAttribute(const FGameplayTag AttributeTag) const;

	UFUNCTION(BlueprintPure , Category= "Attribute")
	bool GetHasAnyAttribute(const TArray<FGameplayTag>& AttributeTags) const;

	UFUNCTION(BlueprintPure , Category= "Attribute")
	bool GetHasAllAttributes(const TArray<FGameplayTag>& AttributeTags) const;

	UFUNCTION(BlueprintPure , Category= "Damage")
	bool CheckCanTakeDamage() const;
};
