// Fill out your copyright notice in the Description page of Project Settings.


#include "EGDamageTrace_Notify.h"

#include "EGDamageTrace_DamageObject.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"


#if ENGINE_MAJOR_VERSION != 5
void UEGDamageTrace_Notify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);	HitActorArray.Empty();
}

void UEGDamageTrace_Notify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);	HitActorArray.Empty();
}
void UEGDamageTrace_Notify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float FrameDeltaTime)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime); Tick(MeshComp,FrameDeltaTime);
}
#endif


void UEGDamageTrace_Notify::Tick(USkeletalMeshComponent* MeshComp, float FrameDeltaTime)
{

	if (!MeshComp && !MeshComp->GetOwner()) return;

	
	TArray<FHitResult> HitResults;
	bool bHit = false;
	switch (NotifyTraceStruct.TraceShape)
	{
	case Sphere : bHit = DrawSphereTrace(MeshComp , HitResults ); break;
	case Box : bHit = DrawBoxTrace(MeshComp , HitResults ); break;
	case Line : bHit = DrawLineTrace(MeshComp , HitResults ); break;
	default: break;
	}
	
	if (bHit)
	{
		for (const auto Hit : HitResults)
		{
			if (!Hit.GetActor()) continue;
	
			if (Hit.GetActor() != MeshComp->GetOwner()  && !HitActorArray.Contains(Hit.GetActor())) 
			{
				HitActorArray.Add(Hit.GetActor());
				HandleDamage(MeshComp , Hit);
				if (bShouldPush)
				{
					if (const auto character = Cast<ACharacter>(Hit.GetActor()))
						character->GetCharacterMovement()->AddForce(UEFMathLibrary::GetDirectionBetweenActors(MeshComp->GetOwner(),character,PushStrength));
				}
			}
		}
	}
}


#if ENGINE_MAJOR_VERSION == 5
void UEGDamageTrace_Notify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference); HitActorArray.Empty();
}

void UEGDamageTrace_Notify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference); HitActorArray.Empty();
}

void UEGDamageTrace_Notify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference); Tick(MeshComp,FrameDeltaTime);
}
#endif






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
				UGameplayStatics::ApplyRadialDamage(GetWorld() ,  NotifyDamageStruct.AnimNotifyDamage , HitResult.Location ,  NotifyDamageStruct.AreaDamageRadius , NotifyDamageStruct.DamageType , IgnoreActors , owner , instigator, true , NotifyDamageStruct.AreaDamageBlockChannel);
			break;
			
			case UEF_ApplyRadialDamageFalloff:
				UGameplayStatics::ApplyRadialDamageWithFalloff(GetWorld(),
					 NotifyDamageStruct.AreaDamageMaximum,
					 NotifyDamageStruct.AreaDamageMinimum ,
					 HitResult.Location ,
					 NotifyDamageStruct.AreaDamageInnerRadius ,
					 NotifyDamageStruct.AreaDamageOuterRadius ,
					 NotifyDamageStruct.AreaDamageFalloff ,
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






