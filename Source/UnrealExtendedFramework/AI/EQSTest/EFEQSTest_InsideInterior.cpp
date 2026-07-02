// Fill out your copyright notice in the Description page of Project Settings.

#include "EFEQSTest_InsideInterior.h"
#include "Components/BoxComponent.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

UEFEQSTest_InsideInterior::UEFEQSTest_InsideInterior(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(false);
	InteriorContext = UEnvQueryContext_Querier::StaticClass();
	BoolValue.DefaultValue = true;
}

void UEFEQSTest_InsideInterior::RunTest(FEnvQueryInstance& QueryInstance) const
{
	TArray<AActor*> ContextActors;
	QueryInstance.PrepareContext(InteriorContext, ContextActors);

	TArray<UBoxComponent*> InteriorBoxes;
	for (AActor* ContextActor : ContextActors)
	{
		if (!ContextActor) continue;

		if (UBoxComponent* FallbackBox = ContextActor->FindComponentByClass<UBoxComponent>())
		{
			InteriorBoxes.Add(FallbackBox);
		}
	}

	const bool bWantsInside = BoolValue.GetValue();

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		// If we couldn't find any interior boxes, instantly fail the point
		if (InteriorBoxes.IsEmpty())
		{
			It.SetScore(TestPurpose, FilterType, false, 0.0f);
			continue;
		}

		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
		bool bIsInsideAny = false;

		for (const UBoxComponent* InteriorBox : InteriorBoxes)
		{
			if (!InteriorBox) continue;

			// Transform world location to box component's local space
			const FVector LocalPoint = InteriorBox->GetComponentTransform().InverseTransformPosition(ItemLocation);
			const FVector BoxExtent = InteriorBox->GetUnscaledBoxExtent();

			if (FMath::Abs(LocalPoint.X) <= BoxExtent.X &&
				FMath::Abs(LocalPoint.Y) <= BoxExtent.Y &&
				FMath::Abs(LocalPoint.Z) <= BoxExtent.Z)
			{
				bIsInsideAny = true;
				break;
			}
		}

		// Calculate if point meets our desired condition (Inside == bWantsInside)
		const bool bConditionMet = (bIsInsideAny == bWantsInside);

		// If condition is met, give score of 1.0f, else 0.0f
		It.SetScore(TestPurpose, FilterType, bConditionMet, bConditionMet ? 1.0f : 0.0f);
	}
}

FText UEFEQSTest_InsideInterior::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Inside Box Interior"));
}

FText UEFEQSTest_InsideInterior::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("Checks whether item locations are inside a context actor box volume"));
}
