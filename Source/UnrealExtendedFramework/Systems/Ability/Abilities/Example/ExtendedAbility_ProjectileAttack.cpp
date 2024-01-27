// Fill out your copyright notice in the Description page of Project Settings.

#include "ExtendedAbility_ProjectileAttack.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"



UExtendedAbility_ProjectileAttack::UExtendedAbility_ProjectileAttack(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}


UExtendedAbility_ProjectileAttack::UExtendedAbility_ProjectileAttack()
{
	AttackAnimDelay = 0.2f;
	HandSocketName = "hand_r";
}



void UExtendedAbility_ProjectileAttack::StartExtendedAbility_Implementation(AActor* Instigator)
{
	Super::StartExtendedAbility_Implementation(Instigator);

	if (const auto Character = Cast<ACharacter>(Instigator))
	{
		Character->PlayAnimMontage(AttackMontage);
		UGameplayStatics::SpawnEmitterAttached(CastParticleEffect ,
			Character->GetMesh() ,
			HandSocketName ,
			FVector::ZeroVector ,
			FRotator::ZeroRotator ,
			EAttachLocation::SnapToTarget);

		FTimerHandle TimerHandle_AttackDelay;
		FTimerDelegate Delegate;
		Delegate.BindUFunction(this,"AttackDelay_Elapsed" , Character);

		GetWorld()->GetTimerManager().SetTimer(TimerHandle_AttackDelay , Delegate , AttackAnimDelay , false);
	}
}


void UExtendedAbility_ProjectileAttack::AttackDelay_Elapsed(ACharacter* InstigatorCharacter)
{
	if (ensureAlways(ProjectileClass) && InstigatorCharacter)
	{
		FVector HandLocation = InstigatorCharacter->GetMesh()->GetSocketLocation(HandSocketName);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.Instigator = InstigatorCharacter;

		FTransform SpawnTransform = FTransform(FRotator::ZeroRotator , HandLocation);

		
		if (const auto Camera = Cast<UCameraComponent>(InstigatorCharacter->GetComponentByClass(UCameraComponent::StaticClass())))
		{

			FCollisionShape Shape;
			Shape.SetSphere(20.f);

			FCollisionQueryParams Params;
			Params.AddIgnoredActor(InstigatorCharacter);

			FCollisionObjectQueryParams ObjectQueryParams;
			ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
			ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
			ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
			
			FVector TraceStart = Camera->GetComponentLocation();
			FVector TraceEnd = Camera->GetComponentLocation() + (InstigatorCharacter->GetControlRotation().Vector() * 5000);

			FHitResult Hit;

			if (GetWorld()->SweepSingleByObjectType(Hit , TraceStart , TraceEnd , FQuat::Identity , ObjectQueryParams , Shape , Params))
				TraceEnd = Hit.ImpactPoint;

			FRotator ProjectileRotation = FRotationMatrix::MakeFromX(TraceEnd - HandLocation).Rotator();
			SpawnTransform = FTransform(ProjectileRotation , HandLocation);
		}

		GetWorld()->SpawnActor<AActor>(ProjectileClass , SpawnTransform , SpawnParameters);
	}
}

