// Fill out your copyright notice in the Description page of Project Settings.

#include "EGProceduralPatrolGeneratorSceneComponent.h"
#include "NavigationSystem.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"


// Sets default values for this component's properties
UEGProceduralPatrolGeneratorSceneComponent::UEGProceduralPatrolGeneratorSceneComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent") );
	SphereComponent->SetupAttachment(this);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent") );
	BoxComponent->SetupAttachment(this);
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void UEGProceduralPatrolGeneratorSceneComponent::UpdateShapes()
{
	if (BoxComponent)
		BoxComponent->SetBoxExtent(BoxCollisionShape);
	
	if (SphereComponent)
		SphereComponent->SetSphereRadius(SphereRadius);
}


void UEGProceduralPatrolGeneratorSceneComponent::UpdatePatrolPoints(UBillboardComponent* BillboardComponent , int32 Index)
{
	if (PatrolPointLocation.Num() != NumberOfPatrolPoints)
	{
		PatrolPointLocation.SetNum(NumberOfPatrolPoints ,true);
	}
	if (BillboardComponent && PatrolPointLocation.IsValidIndex(Index))
	{
		PatrolPointComponents.Add(BillboardComponent);
		BillboardComponent->SetRelativeLocation(PatrolPointLocation[Index]);
		if (bCheckNavMesh)
		{
			BillboardComponent->SetSprite(CheckIfPointIsReachable(BillboardComponent->GetComponentLocation()) ? PatrolPointAcceptedTexture : PatrolPointRefusedTexture);
		}
		else
		{
			BillboardComponent->SetSprite(PatrolPointAcceptedTexture);
		}
	}
}



void UEGProceduralPatrolGeneratorSceneComponent::ClearPatrolPoints()
{
	for (const auto PPC : PatrolPointComponents)
	{
		if (PPC)
			PPC->DestroyComponent();
	}
	PatrolPointComponents.Empty();
}


void UEGProceduralPatrolGeneratorSceneComponent::GeneratePoints()
{
	for (int32 i = 0 ; i < NumberOfPatrolPoints ; i++)
	{
		int32 TryCount = 0;
		bool Found = false;
		
		while (TryCount < 25 && !Found)
		{
			TryCount++;
			FVector RandomLocation =  GetRandomRelativeLocation();
			if (bCheckNavMesh && !CheckIfPointIsReachable(LocalToWorld(RandomLocation)))
				continue;
			
			for (const auto Locations : PatrolPointLocation)
			{
				if (!RandomLocation.Equals(Locations,DistanceBetweenPatrolPoints))
				{
					Found = true;
					PatrolPointLocation[i] = RandomLocation;
					break;
				}
			}
		}
		if (!Found)
		{
			PatrolPointLocation[i] = GetRandomRelativeLocation();
		}
	}
}

bool UEGProceduralPatrolGeneratorSceneComponent::CheckIfPointIsReachable(const FVector& Point) const
{
	if (NavSys)
	{
		FNavLocation Location;
		return NavSys->ProjectPointToNavigation(Point , Location);
	}
	return false;
}



void UEGProceduralPatrolGeneratorSceneComponent::GeneratePatrolPoints_Implementation()
{
	if (!NavSys)
	{
		NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	}
	
	GeneratePoints();
	
	if (NavSys)
	{
		FLineTraceStruct LineTraceStruct;
		for (int32 i = 0 ; i < NumberOfPatrolPoints ; i++)
		{
			LineTraceStruct.Start = GetComponentLocation() + PatrolPointLocation[i] + FVector{0,0,GetTraceSize()};
			LineTraceStruct.End = GetComponentLocation() + PatrolPointLocation[i] - FVector{0,0,GetTraceSize()};
			
			if (UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),LineTraceStruct))
			{
				FNavLocation NavLocation;
				
				if (NavSys->GetRandomPointInNavigableRadius(LineTraceStruct.GetHitLocation(),25,NavLocation))
				{
					PatrolPointLocation[i] = WorldToLocal(NavLocation.Location);
				}
			}
		}
	}
	
	for (int32 i = 0 ; i < NumberOfPatrolPoints ; i++)
	{
		PatrolPointComponents[i]->SetRelativeLocation(PatrolPointLocation[i]);
	}
}




FVector UEGProceduralPatrolGeneratorSceneComponent::WorldToLocal(const FVector& WorldLocation) const
{
	return UKismetMathLibrary::InverseTransformLocation(GetComponentTransform(), WorldLocation);
}

FVector UEGProceduralPatrolGeneratorSceneComponent::LocalToWorld(const FVector& LocalLocation) const
{
	return UKismetMathLibrary::TransformLocation(GetComponentTransform(), LocalLocation);
}


float UEGProceduralPatrolGeneratorSceneComponent::GetTraceSize() const
{
	if (PatrolShape == EProceduralPatrolShape::Box)
		return BoxCollisionShape.Z * 1.5;
	
	if (PatrolShape == EProceduralPatrolShape::Sphere)
		return SphereRadius * 1.5;
	
	return 1000;
}


FVector UEGProceduralPatrolGeneratorSceneComponent::GetRandomRelativeLocation() const
{
	if (PatrolShape == EProceduralPatrolShape::Box)
	{
		const FVector Location = UKismetMathLibrary::RandomPointInBoundingBox(GetComponentLocation() , BoxCollisionShape);
		const FVector Relative = WorldToLocal(Location); 
		return Relative;
	}
	if (PatrolShape == EProceduralPatrolShape::Sphere)
	{
		const FVector Location = UEFMathLibrary::RandPointInSphere(GetComponentLocation() , SphereRadius);
		const FVector Relative = WorldToLocal(Location);
		return Relative;
	}
	return FVector{0,0,0};
}