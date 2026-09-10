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
	BoolValue.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
	const bool bWantsInside = BoolValue.GetValue();

	TArray<AActor*> ContextActors;
	QueryInstance.PrepareContext(InteriorContext, ContextActors);

	TArray<UBoxComponent*> InteriorBoxes;
	for (AActor* ContextActor : ContextActors)
	{
		if (!IsValid(ContextActor))
		{
			continue;
		}

		if (UBoxComponent* InteriorBox = ContextActor->FindComponentByClass<UBoxComponent>())
		{
			InteriorBoxes.Add(InteriorBox);
		}
	}

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		bool bIsInsideAny = false;
		if (!InteriorBoxes.IsEmpty())
		{
			const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
			for (const UBoxComponent* InteriorBox : InteriorBoxes)
			{
				if (!InteriorBox)
				{
					continue;
				}

				// Local space already includes component scale, so compare against the unscaled extent.
				// Interior volumes are XY footprints; skip Z so navmesh-projected points still count.
				const FVector LocalPoint = InteriorBox->GetComponentTransform().InverseTransformPosition(ItemLocation);
				const FVector BoxExtent = InteriorBox->GetUnscaledBoxExtent();
				if (FMath::Abs(LocalPoint.X) <= BoxExtent.X &&
					FMath::Abs(LocalPoint.Y) <= BoxExtent.Y)
				{
					bIsInsideAny = true;
					break;
				}
			}
		}

		It.SetScore(TestPurpose, FilterType, bIsInsideAny, bWantsInside);
	}
}

FText UEFEQSTest_InsideInterior::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Inside Box Interior"));
}

FText UEFEQSTest_InsideInterior::GetDescriptionDetails() const
{
	return DescribeBoolTestParams(TEXT("inside box interior"));
}
