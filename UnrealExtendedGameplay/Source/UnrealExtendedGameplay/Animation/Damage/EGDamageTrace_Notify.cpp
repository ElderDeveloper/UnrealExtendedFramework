// Fill out your copyright notice in the Description page of Project Settings.


#include "EGDamageTrace_Notify.h"

#include "EGDamageTrace_DamageObject.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"




UEGDamageTrace_Notify::UEGDamageTrace_Notify()
{
	PSTemplate = nullptr;
	OwnerMesh = nullptr;
}

void UEGDamageTrace_Notify::Tick(USkeletalMeshComponent* MeshComp, float FrameDeltaTime)
{
	OwnerMesh = MeshComp;
	if (UseEngineTickForCollisionCalculation)
		DamageTick();
}

void UEGDamageTrace_Notify::DamageTick()
{
	if (!OwnerMesh) return;
	if (!OwnerMesh->GetOwner()) return;
	
	TArray<FHitResult> HitResults;
	bool bHit = false;
	switch (NotifyTraceStruct.TraceShape)
	{
	case Sphere : bHit = DrawSphereTrace(OwnerMesh , HitResults ); break;
	case Box : bHit = DrawBoxTrace(OwnerMesh , HitResults ); break;
	case Line : bHit = DrawLineTrace(OwnerMesh , HitResults ); break;
	default: break;
	}
	
	if (bHit)
	{
		for (const auto Hit : HitResults)
		{
			if (!Hit.GetActor()) continue;
	
			if (Hit.GetActor() != OwnerMesh->GetOwner()  && !HitActorArray.Contains(Hit.GetActor())) 
			{
				HitActorArray.Add(Hit.GetActor());
				HandleDamage(OwnerMesh , Hit);
				if (bShouldPush)
				{
					if (const auto character = Cast<ACharacter>(Hit.GetActor()))
						character->GetCharacterMovement()->AddForce(UEFMathLibrary::GetDirectionBetweenActors(OwnerMesh->GetOwner(),character,PushStrength));
				}
			}
		}
	}
}



void UEGDamageTrace_Notify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	HitActorArray.Empty();
	OwnerMesh = MeshComp;

	if (!UseEngineTickForCollisionCalculation)
	{
		if (MeshComp)
		{
			DamageTick();
			if (MeshComp->GetWorld())
				MeshComp->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UEGDamageTrace_Notify::DamageTick,CollisionCalculationTickSpeed,true);
		}
	}
}

void UEGDamageTrace_Notify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference); HitActorArray.Empty();
	
	if(MeshComp && MeshComp->GetWorld())
	{
		MeshComp->GetWorld()->GetTimerManager().ClearTimer(Handle);
	}
}

void UEGDamageTrace_Notify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference); Tick(MeshComp,FrameDeltaTime);
}

void UEGDamageTrace_Notify::HandleDamage(USkeletalMeshComponent* MeshComp , FHitResult HitResult)
{
	
	if (NotifyDamageStruct.bApplyAnimNotifyDamage)
	{
		const auto instigator =  MeshComp->GetOwner()->GetInstigatorController();
		const auto owner = MeshComp->GetOwner();
		TArray<AActor*> IgnoreActors;
		IgnoreActors.Add(MeshComp->GetOwner());
		
		
		switch (NotifyDamageStruct.ApplyDamageType)
		{
			case UEF_ApplyDamage: 	UGameplayStatics::ApplyDamage(HitResult.GetActor() , NotifyDamageStruct.AnimNotifyDamage ,instigator,owner,NotifyDamageStruct.DamageType); break;
			
			case UEF_ApplyPointDamage: UGameplayStatics::ApplyPointDamage(HitResult.GetActor() ,  NotifyDamageStruct.AnimNotifyDamage , HitResult.Location , HitResult , instigator,owner,NotifyDamageStruct.DamageType); break;
			
			case UEF_ApplyRadialDamage:
				UGameplayStatics::ApplyRadialDamage(GetWorld() ,  NotifyDamageStruct.AnimNotifyDamage , HitResult.Location ,  NotifyDamageStruct.RadialDamageStruct.AreaDamageRadius , NotifyDamageStruct.DamageType , IgnoreActors , owner , instigator, true , NotifyDamageStruct.AreaDamageBlockChannel);
			break;
			
			case UEF_ApplyRadialDamageFalloff:
				UGameplayStatics::ApplyRadialDamageWithFalloff(GetWorld(),
					 NotifyDamageStruct.RadialDamageStruct.AreaDamageMaximum,
					 NotifyDamageStruct.RadialDamageStruct.AreaDamageMinimum ,
					 HitResult.Location ,
					 NotifyDamageStruct.RadialDamageStruct.AreaDamageInnerRadius ,
					 NotifyDamageStruct.RadialDamageStruct.AreaDamageOuterRadius ,
					 NotifyDamageStruct.RadialDamageStruct.AreaDamageFalloff ,
					 NotifyDamageStruct.DamageType,
					 IgnoreActors ,
					 owner ,
					 instigator ,
					 NotifyDamageStruct.AreaDamageBlockChannel );
			break;
			default:break;
		}
	}
	
	else if (NotifyDamageStruct.CustomApplyDamageObject)
	{
		if(const auto CustomDamageObject = NewObject<UEGDamageTrace_DamageObject>(NotifyDamageStruct.CustomApplyDamageObject))
		{
			CustomDamageObject->ReceiveApplyDamageInformation(MeshComp , HitResult);
		}
	}
}

bool UEGDamageTrace_Notify::DrawSphereTrace(USkeletalMeshComponent* MeshComp, TArray<FHitResult>& HitResults)
{
	NotifyTraceStruct.SphereTraceSettings.Start = MeshComp->GetSocketLocation(FirstSocketName);
	NotifyTraceStruct.SphereTraceSettings.End = MeshComp->GetSocketLocation(SecondSocketName);
	if(UEFTraceLibrary::ExtendedSphereTraceMulti(MeshComp->GetWorld(),NotifyTraceStruct.SphereTraceSettings))
	{
		HitResults = NotifyTraceStruct.SphereTraceSettings.HitResults;
		return true;
	}
	return false;
}

bool UEGDamageTrace_Notify::DrawBoxTrace(USkeletalMeshComponent* MeshComp, TArray<FHitResult>& HitResults)
{
	NotifyTraceStruct.BoxTraceSettings.Start = MeshComp->GetSocketLocation(FirstSocketName);
	NotifyTraceStruct.BoxTraceSettings.End = MeshComp->GetSocketLocation(SecondSocketName);
	if(UEFTraceLibrary::ExtendedBoxTraceMulti(MeshComp->GetWorld(),NotifyTraceStruct.BoxTraceSettings))
	{
		HitResults = NotifyTraceStruct.BoxTraceSettings.HitResults;
		return true;
	}
	return false;
}

bool UEGDamageTrace_Notify::DrawLineTrace(USkeletalMeshComponent* MeshComp, TArray<FHitResult>& HitResults)
{
	NotifyTraceStruct.LineTraceSettings.Start = MeshComp->GetSocketLocation(FirstSocketName);
	NotifyTraceStruct.LineTraceSettings.End = MeshComp->GetSocketLocation(SecondSocketName);
	if(UEFTraceLibrary::ExtendedLineTraceMulti(MeshComp->GetWorld(),NotifyTraceStruct.LineTraceSettings))
	{
		HitResults = NotifyTraceStruct.LineTraceSettings.HitResults;
		return true;
	}
	return false;
}






