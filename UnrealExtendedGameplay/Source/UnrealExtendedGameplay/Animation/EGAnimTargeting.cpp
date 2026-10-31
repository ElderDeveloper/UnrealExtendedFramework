// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimTargeting.h"

#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "UnrealExtendedGameplay/Targeting/EGDynamicTargetingComponent.h"
#include "UnrealExtendedGameplay/Targeting/EGTargetingComponent.h"

AActor* EGAnimTargeting::GetLockedTarget(const AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}

	if (const UEGTargetingComponent* Targeting = Owner->FindComponentByClass<UEGTargetingComponent>())
	{
		if (AActor* Target = Targeting->GetLockedOnTargetActor(); IsValid(Target))
		{
			return Target;
		}
	}

	if (const UEGDynamicTargetingComponent* Targeting = Owner->FindComponentByClass<UEGDynamicTargetingComponent>())
	{
		if (AActor* Target = Targeting->GetLockedOnTargetActor(); IsValid(Target))
		{
			return Target;
		}
	}

	return nullptr;
}

AActor* EGAnimTargeting::FindClosestPawn(const AActor* Owner, float Radius, float HalfAngleDegrees, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes)
{
	UWorld* const World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || Radius <= 0.0f || ObjectTypes.Num() == 0)
	{
		return nullptr;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EGAnimTargeting), false, Owner);
	TArray<AActor*> Attached;
	Owner->GetAttachedActors(Attached, true, true);
	QueryParams.AddIgnoredActors(Attached);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Owner->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ObjectTypes), FCollisionShape::MakeSphere(Radius), QueryParams);

	const FVector Origin = Owner->GetActorLocation();
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f)));

	AActor* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* const Pawn = Cast<APawn>(Overlap.GetActor());
		if (!IsValid(Pawn) || Pawn == Owner)
		{
			continue;
		}

		const FVector ToPawn = Pawn->GetActorLocation() - Origin;
		if (HalfAngleDegrees < 180.0f && FVector::DotProduct(Forward, ToPawn.GetSafeNormal2D()) < MinDot)
		{
			continue;
		}

		const double DistSq = ToPawn.SizeSquared();
		if (DistSq < BestDistSq)
		{
			Best = Pawn;
			BestDistSq = DistSq;
		}
	}
	return Best;
}

AActor* EGAnimTargeting::FindTarget(const AActor* Owner, float Radius, float HalfAngleDegrees, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes)
{
	if (AActor* Locked = GetLockedTarget(Owner))
	{
		return Locked;
	}
	return FindClosestPawn(Owner, Radius, HalfAngleDegrees, ObjectTypes);
}
