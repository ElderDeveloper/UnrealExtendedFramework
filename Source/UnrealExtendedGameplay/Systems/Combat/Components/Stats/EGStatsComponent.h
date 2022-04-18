// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnrealExtendedFramework/Data/EFStructs.h"
#include "EGStatsComponent.generated.h"


#define DEATH_RETURN() return

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthUpdateDelegate , float , CurrentHealth , float , MaximumHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShieldUpdateDelegate , float , CurrentShield , float , MaximumShield);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTakeDamageDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStatsPlayMontageBlueprint , UAnimMontage* , Montage);


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) , BlueprintType , Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGStatsComponent : public UActorComponent
{
	GENERATED_BODY()

	UEGStatsComponent();
	
	
protected:

	//<<<<<<<<<<<<<<<<<<<<<<<< HEALTH >>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Health")
	bool CanTakeHealthDamage = true;
	
	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Health")
	float MaxHealth = 100;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Health|Refill")
	bool CanHealthAutoFill = false;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Health|Refill")
	float HealthFillDelay = 2;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Health|Refill")
	float HealthFillSpeed = 0.05;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Health|Refill")
	float HealthFillPercent = 10;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Health|Debug")
	bool PrintDebugHealth = false;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Health|Debug")
	FColor PrintDebugHealthColor = FColor::Cyan;
	

	//<<<<<<<<<<<<<<<<<<<<<< SHIELD >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Shield")
	bool HasShield = false;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Shield")
	bool CanTakeShieldDamage = true;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Shield")
	float MaxShield = 100;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Shield|Refill")
	bool CanShieldAutoFill = true;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Shield|Refill")
	float ShieldFillDelay = 1.5;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Shield|Refill")
	float ShieldFillSpeed = 0.05;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Shield|Refill")
	float ShieldFillPercent = 10;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Shield|Debug")
	bool PrintDebugShield = false;

	UPROPERTY(EditDefaultsOnly,Category="Stats Component|Shield|Debug")
	FColor PrintDebugShieldColor = FColor::Cyan;
	

	//<<<<<<<<<<<<<<<<<<<<<<<<<< ARMOR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	UPROPERTY(EditDefaultsOnly, Category="Stats Component|Damage")
	FVector2D DamageReducePercent = { 3.f, 5.f };

	UPROPERTY(EditDefaultsOnly, Category="Stats Component|Damage")
	float MinDamageReceived = 1.f;


	//<<<<<<<<<<<<<<<<<<<<<<<<<< MONTAGES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Stats Component|Montages")
	TArray<FExtendedFrameworkDamageReaction> ImpactReactions;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Stats Component|Montages")
	TArray<FExtendedFrameworkDamageReaction> DeathMontages;

	
	
	bool IsAlive = true;
	float CurrentHealth = 100;
	float CurrentShield = 100;

	FTimerHandle HealthDelayHandle;
	FTimerHandle HealthFillHandle;
	FTimerHandle ShieldDelayHandle;
	FTimerHandle ShieldFillHandle;


	
public:

	UPROPERTY(BlueprintAssignable)
	FOnHealthUpdateDelegate OnHealthUpdateDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnShieldUpdateDelegate OnShieldUpdateDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnTakeDamageDelegate OnTakeDamageDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnDeathDelegate OnDeathDelegate;

	UPROPERTY(BlueprintAssignable)
	FStatsPlayMontageBlueprint StatsPlayMontageBlueprint;
	


protected:

	UFUNCTION()
	void OnTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);
	UFUNCTION()
	void OnTakePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser);
	UFUNCTION()
	void OnTakeRadialDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, FVector Origin, FHitResult HitInfo, class AController* InstigatedBy, AActor* DamageCauser);


	
	UFUNCTION()
	void HealthRegenDelayEvent();
	UFUNCTION()
	void HealthRegenTimer();

	
	UFUNCTION()
	void ShieldRegenDelayEvent();
	UFUNCTION()
	void ShieldRegenTimer();


	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	bool RemoveHeathCheckIsAlive(const float Damage);
	void HealthRegenTrigger();
	void ShieldRegenTrigger();



	void PlayDamageMontages(const UDamageType* DamageType , FHitResult HitInfo, AActor* DamageCauser);
	void PlayDeathMontages(const UDamageType* DamageType , FHitResult HitInfo, AActor* DamageCauser);
	UAnimMontage* GetDamageMontage (const UDamageType* DamageType, AActor* DamageCauser , const TArray<FExtendedFrameworkDamageReaction>& Animations) const;




	
public:



	UFUNCTION(BlueprintCallable)
	void StatsCustomTakeDamage(const float Damage);

	
	UFUNCTION(BlueprintPure , Category="ExtendedStatsComponent|Alive")
	FORCEINLINE bool GetIsAlive() const { return IsAlive; }
	
	
	UFUNCTION(BlueprintPure , Category="ExtendedStatsComponent|Health")
	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure , Category="ExtendedStatsComponent|Health")
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure , Category="ExtendedStatsComponent|Health")
	FORCEINLINE float GetHealthPercentage() const { return CurrentHealth/MaxHealth; }

	UFUNCTION(BlueprintPure , Category="ExtendedStatsComponent|Health")
	FORCEINLINE bool GetCanTakeHealthDamage() const { return CanTakeHealthDamage; }
	
	
	
	UFUNCTION(BlueprintPure , Category="ExtendedStatsComponent|Shield")
	FORCEINLINE bool GetHasShield() const { return HasShield; }
	
	UFUNCTION(BlueprintPure, Category="ExtendedStatsComponent|Shield")
	FORCEINLINE float GetCurrentShield() const { return CurrentShield; }

	UFUNCTION(BlueprintPure, Category="ExtendedStatsComponent|Shield")
	FORCEINLINE float GetMaxShield() const { return MaxShield; }

	UFUNCTION(BlueprintPure, Category="ExtendedStatsComponent|Shield")
	FORCEINLINE float GetShieldPercentage() const { return CurrentShield/MaxShield; }

	UFUNCTION(BlueprintPure , Category="ExtendedStatsComponent|Shield")
	FORCEINLINE bool GetCanTakeShieldDamage() const { return CanTakeShieldDamage; }
	
};
