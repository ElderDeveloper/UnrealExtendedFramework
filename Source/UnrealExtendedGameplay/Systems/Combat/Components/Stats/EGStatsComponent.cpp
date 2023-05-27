// Fill out your copyright notice in the Description page of Project Settings.


#include "EGStatsComponent.h"
#include "GameFramework/Character.h"
#include "UnrealExtendedFramework/Data/EFMacro.h"
#include "UnrealExtendedFramework/Libraries/Array/UEFArrayLibrary.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"


// Sets default values for this component's properties
UEGStatsComponent::UEGStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}



// Called when the game starts
void UEGStatsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const auto Actor = Cast<AActor>(GetOwner()))
	{
		Actor->OnTakeAnyDamage.AddDynamic(this,&UEGStatsComponent::OnTakeAnyDamage);
		Actor->OnTakePointDamage.AddDynamic(this,&UEGStatsComponent::OnTakePointDamage);
	}
	CurrentShield = MaxShield;
	CurrentHealth = MaxHealth;

	OnHealthUpdateDelegate.Broadcast(CurrentHealth,MaxHealth);
	OnShieldUpdateDelegate.Broadcast(CurrentShield,MaxShield);
}




void UEGStatsComponent::StatsCustomTakeDamage(const float Damage)
{
	RemoveHeathCheckIsAlive(Damage);

	FHitResult LocalHitInfo;
	LocalHitInfo.Location = GetOwner()->GetActorLocation();
	LocalHitInfo.TraceStart = GetOwner()->GetActorLocation();

	const UDamageType* DamageType = nullptr;
	AActor* DamageCauser = nullptr;
	
	if(GetIsAlive())
		PlayDamageMontages(DamageType , LocalHitInfo,DamageCauser);
	else
		PlayDeathMontages(DamageType , LocalHitInfo , DamageCauser);


}

void UEGStatsComponent::OnTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,AController* InstigatedBy, AActor* DamageCauser)
{
	RemoveHeathCheckIsAlive(Damage);
}



void UEGStatsComponent::OnTakePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy,FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection,const UDamageType* DamageType, AActor* DamageCauser)
{
	FHitResult LocalHitInfo;
	LocalHitInfo.Location = HitLocation;
	LocalHitInfo.TraceStart = ShotFromDirection;
	LocalHitInfo.BoneName = BoneName;
	
	if(GetIsAlive())
		PlayDamageMontages(DamageType , LocalHitInfo,DamageCauser);
	else
		PlayDeathMontages(DamageType , LocalHitInfo , DamageCauser);
}



void UEGStatsComponent::OnTakeRadialDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,FVector Origin, FHitResult HitInfo, AController* InstigatedBy, AActor* DamageCauser)
{
	if (GetIsAlive())
		PlayDamageMontages(DamageType , HitInfo ,DamageCauser);
	else
		PlayDeathMontages(DamageType , HitInfo , DamageCauser);
}




bool UEGStatsComponent::RemoveHeathCheckIsAlive(const float Damage)
{
	float LessenedDamage = Damage;
	LessenedDamage -= (Damage * FMath::RandRange(DamageReducePercent.X,DamageReducePercent.Y)) / 100;
	
	const float RealDamage = FMath::Clamp<float>(LessenedDamage , MinDamageReceived , 99999999999.f);
	
	if(HasShield && CurrentShield > 0 && CanTakeShieldDamage)
	{
		CurrentShield = CurrentShield - RealDamage;

		if (CurrentShield < 0)
		{
			CurrentHealth -= FMath::Clamp<float>(CurrentHealth- (CurrentShield * -1) , 0 , MaxHealth);
			CurrentShield = 0;

			OnShieldUpdateDelegate.Broadcast(CurrentShield,MaxShield);
			OnHealthUpdateDelegate.Broadcast(CurrentHealth,MaxHealth);
			OnTakeDamageDelegate.Broadcast();
			
			IsAlive = CurrentHealth > 0;

			if (IsAlive)
			{
				HealthRegenTrigger();
				ShieldRegenTrigger();
			}
			return IsAlive;
		}
		ShieldRegenTrigger();
		return true;
	}

	if (CanTakeHealthDamage && CanTakeShieldDamage)
	{
		CurrentHealth = FMath::Clamp<float>(CurrentHealth-RealDamage , 0 , MaxHealth);
		OnHealthUpdateDelegate.Broadcast(CurrentHealth,MaxHealth);
		OnTakeDamageDelegate.Broadcast();
		
		IsAlive = CurrentHealth > 0;
		
		if (IsAlive)
		{
			HealthRegenTrigger();
			ShieldRegenTrigger();
			return IsAlive;
		}
		OnDeathDelegate.Broadcast();
	}
	return IsAlive;
}



void UEGStatsComponent::PlayDamageMontages(const UDamageType* DamageType, FHitResult HitInfo, AActor* DamageCauser)
{
	if (const auto Montage = GetDamageMontage(DamageType,DamageCauser , ImpactReactions))
	{
		if (StatsPlayMontageBlueprint.IsBound())
			StatsPlayMontageBlueprint.Broadcast(Montage);
		
		else if (const auto Character = Cast<ACharacter>(GetOwner()))
			Character->GetMesh()->GetAnimInstance()->Montage_Play(Montage);
	}
}



void UEGStatsComponent::PlayDeathMontages(const UDamageType* DamageType, FHitResult HitInfo, AActor* DamageCauser)
{
	if (const auto Montage = GetDamageMontage(DamageType,DamageCauser , DeathMontages))
	{
		if (StatsPlayMontageBlueprint.IsBound())
			StatsPlayMontageBlueprint.Broadcast(Montage);
		
		else if (const auto Character = Cast<ACharacter>(GetOwner()))
			Character->GetMesh()->GetAnimInstance()->Montage_Play(Montage);
	}
}



bool GetRandom(const TArray<FExtendedFrameworkDamageReaction>& Array , FExtendedFrameworkDamageReaction& Item)
{
	if (Array.Num() > 0)
	{
		Item = Array[UUEFArrayLibrary::GetRandomArrayIndex(Array.Num()-1)];
		return true;
	}
	return false;
}


UAnimMontage* UEGStatsComponent::GetDamageMontage(const UDamageType* DamageType, AActor* DamageCauser,const TArray<FExtendedFrameworkDamageReaction>& Animations) const
{
	FExtendedFrameworkDamageReaction FoundStruct = {};
	if (DamageCauser)
	{
		TArray<FExtendedFrameworkDamageReaction> FoundStructs;
		
		const auto Direction = UEFMathLibrary::CalculateDirectionBetweenActors(GetOwner(),DamageCauser);
		
		for (const auto i : Animations)
		{
			if (i.DamageDirection == Direction)
			{
				FoundStructs.Add(i);
			}
		}

		if (DamageType)
		{
			for (const auto i : FoundStructs)
			{
				if (i.DamageType == DamageType->GetClass())
					return FoundStruct.MontageReaction;
			}
		}

		if(GetRandom(FoundStructs , FoundStruct))
			return FoundStruct.MontageReaction;
		
		if(GetRandom(Animations , FoundStruct))
			return FoundStruct.MontageReaction;
		
	}

	if (DamageType)
	{
		for (const auto i : Animations)
		{
			if (i.DamageType == DamageType->GetClass())
			{
				return FoundStruct.MontageReaction;
			}
		}
	}
	
	if(GetRandom(Animations , FoundStruct))
		return FoundStruct.MontageReaction;
	
	return nullptr;
}




void UEGStatsComponent::HealthRegenTrigger()
{
	if (CanHealthAutoFill)
	{
		CLEAR_TIMER(HealthDelayHandle);
		CLEAR_TIMER(HealthFillHandle);
		CREATE_TIMER(HealthDelayHandle , this,&UEGStatsComponent::HealthRegenDelayEvent,HealthFillDelay,false);
	}
}

void UEGStatsComponent::HealthRegenDelayEvent()
{
	if (!GetIsAlive()) return;
	HealthRegenTimer();
	CREATE_TIMER(HealthFillHandle,this,&UEGStatsComponent::HealthRegenTimer,HealthFillSpeed,true);
}

void UEGStatsComponent::HealthRegenTimer()
{
	if (!GetIsAlive()) return;

	CurrentHealth += MaxHealth*HealthFillPercent/100;
	OnHealthUpdateDelegate.Broadcast(CurrentHealth,MaxHealth);
	if (CurrentHealth >= MaxHealth)
	{
		CurrentHealth=MaxHealth;
		CLEAR_TIMER(HealthFillHandle);
	}
}





void UEGStatsComponent::ShieldRegenTrigger()
{
	if(CanShieldAutoFill)
	{
		CLEAR_TIMER(ShieldDelayHandle);
		CLEAR_TIMER(ShieldFillHandle);
		CREATE_TIMER(ShieldDelayHandle,this,&UEGStatsComponent::ShieldRegenDelayEvent,ShieldFillDelay,false);
	}
}


void UEGStatsComponent::ShieldRegenDelayEvent()
{
	if (!GetIsAlive()) return;
	ShieldRegenTimer();
	CREATE_TIMER(ShieldFillHandle,this,&UEGStatsComponent::ShieldRegenTimer,ShieldFillSpeed,true);
}

void UEGStatsComponent::ShieldRegenTimer()
{
	if (!GetIsAlive()) return;

	CurrentShield = FMath::Clamp<float>(CurrentShield + MaxShield*ShieldFillPercent/100,0,MaxShield);
	
	if (CurrentShield >= MaxShield)
	{
		CurrentShield=MaxShield;
		CLEAR_TIMER(ShieldFillHandle);
	}
	OnShieldUpdateDelegate.Broadcast(CurrentShield,MaxShield);
}






void UEGStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (PrintDebugHealth)
		GEngine->AddOnScreenDebugMessage(-1,0,PrintDebugHealthColor,FString::Printf( TEXT("%s : Current Health: %f , Max Health: %f ") , *GetOwner()->GetName() , CurrentHealth , MaxHealth));

	if (PrintDebugShield)
		GEngine->AddOnScreenDebugMessage(-1,0,PrintDebugShieldColor,FString::Printf( TEXT("%s : Current Shield: %f , Max Shield: %f ") , *GetOwner()->GetName() , CurrentShield , MaxShield));
	

}