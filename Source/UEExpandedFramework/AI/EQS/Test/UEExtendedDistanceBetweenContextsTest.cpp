// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedDistanceBetweenContextsTest.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

#define ENVQUERYTEST_DISTANCE_NAN_DETECTION 1

namespace
{
	FORCEINLINE float CalcDistance3D(const FVector& PosA, const FVector& PosB)
	{
		return (PosB - PosA).Size();
	}
	
	FORCEINLINE void CheckItemLocationForNaN(const FVector& ItemLocation, UObject* QueryOwner, int32 Index, uint8 TestMode)
	{
#if ENVQUERYTEST_DISTANCE_NAN_DETECTION
		ensureMsgf(!ItemLocation.ContainsNaN(), TEXT("EnvQueryTest_Distance NaN in ItemLocation with owner %s. X=%f,Y=%f,Z=%f. Index:%d, TesMode:%d"), *GetPathNameSafe(QueryOwner), ItemLocation.X, ItemLocation.Y, ItemLocation.Z, Index, TestMode);
#endif
	}
}



void UUEExtendedDistanceBetweenContextsTest::FindActor(AActor*& Actor)
{

}

UUEExtendedDistanceBetweenContextsTest::UUEExtendedDistanceBetweenContextsTest(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
}











void UUEExtendedDistanceBetweenContextsTest::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AActor* QueryOwner = Cast<AActor>(QueryInstance.Owner.Get());
	
	if (QueryOwner == nullptr)
	{
		return;
	}

	FloatValueMin.BindData(QueryOwner, QueryInstance.QueryID);
	float MinThresholdValue = FloatValueMin.GetValue();

	FloatValueMax.BindData(QueryOwner, QueryInstance.QueryID);
	float MaxThresholdValue = FloatValueMax.GetValue();


	if (ToPlayer)
	{
		if (const auto Player = UGameplayStatics::GetPlayerPawn(QueryInstance.World,0))
		{
			const float BetweenDistance = CalcDistance3D(QueryOwner->GetActorLocation(), Player->GetActorLocation());
			for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
			{
				const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
				CheckItemLocationForNaN(ItemLocation, QueryOwner, It.GetIndex(), 0);
				const float distance = CalcDistance3D(ItemLocation , Player->GetActorLocation());
				const float score = UKismetMathLibrary::MapRangeClamped(distance,MinDistance,BetweenDistance,MaxThresholdValue,MinThresholdValue);
				It.SetScore(TestPurpose,FilterType,score,MinThresholdValue, MaxThresholdValue);
			}
			return;
		}
	}


	
	else
	{


		
		TArray<AActor*> Array;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), SearchActor, Array);
		AActor* TargetActor = nullptr;
		for (const auto i : Array)
		{
			TargetActor = i;
			break;
		}


		
		if (TargetActor)
		{



			
			const float BetweenDistance = CalcDistance3D(QueryOwner->GetActorLocation(), TargetActor->GetActorLocation());
			
			for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
			{
				const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
				CheckItemLocationForNaN(ItemLocation, QueryOwner, It.GetIndex(), 0);

				
				const float DistanceToTarget = CalcDistance3D(ItemLocation , TargetActor->GetActorLocation());
				const float DistanceToOwner = CalcDistance3D(ItemLocation , QueryOwner->GetActorLocation());

				float score = UKismetMathLibrary::MapRangeClamped(DistanceToTarget,MinDistance,MaxDistance , 1,0);

				
                const float scoreMultiply = UKismetMathLibrary::MapRangeClamped(DistanceToOwner,0,DistanceToTarget,0,1);

				
				score -= scoreMultiply;

				
				It.SetScore(TestPurpose,FilterType,score,MinThresholdValue, MaxThresholdValue);



				
			}
			return;
		}

		
		
	}
	GEngine->AddOnScreenDebugMessage(-1,1.f,FColor::Red,"Error");
	
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
		
		CheckItemLocationForNaN(ItemLocation, QueryOwner, It.GetIndex(), 0);

		It.SetScore(TestPurpose,FilterType,100,MinThresholdValue, MaxThresholdValue);
				
	}
}








FText UUEExtendedDistanceBetweenContextsTest::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("From: To")));
}

FText UUEExtendedDistanceBetweenContextsTest::GetDescriptionDetails() const
{
	return FText::FromString("From , To");
}
