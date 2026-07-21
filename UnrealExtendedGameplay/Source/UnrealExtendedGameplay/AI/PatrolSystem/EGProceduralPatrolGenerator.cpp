// Fill out your copyright notice in the Description page of Project Settings.

#include "EGProceduralPatrolGenerator.h"


#include "NavigationSystem.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"


AEGProceduralPatrolGenerator::AEGProceduralPatrolGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;
}


void AEGProceduralPatrolGenerator::UpdateShapes()
{
	if (BoxComponent)
		BoxComponent->SetBoxExtent(BoxCollisionShape);
	
	if (SphereComponent)
		SphereComponent->SetSphereRadius(SphereRadius);
}


void AEGProceduralPatrolGenerator::UpdatePatrolPoints(UBillboardComponent* BillboardComponent , int32 Index)
{
	if (PatrolPointLocation.Num() != NumberOfPatrolPoints)
	{
		PatrolPointLocation.SetNum(NumberOfPatrolPoints ,EAllowShrinking::Yes);
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



void AEGProceduralPatrolGenerator::ClearPatrolPoints()
{
	for (const auto PPC : PatrolPointComponents)
	{
		if (PPC)
			PPC->DestroyComponent();
	}
	PatrolPointComponents.Empty();
}


void AEGProceduralPatrolGenerator::GeneratePoints()
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

bool AEGProceduralPatrolGenerator::CheckIfPointIsReachable(const FVector& Point) const
{
	if (NavSys)
	{
		FNavLocation Location;
		return NavSys->ProjectPointToNavigation(Point , Location);
	}
	return false;
}



void AEGProceduralPatrolGenerator::GeneratePatrolPoints_Implementation()
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
			LineTraceStruct.Start = GetActorLocation() + PatrolPointLocation[i] + FVector{0,0,GetTraceSize()};
			LineTraceStruct.End = GetActorLocation() + PatrolPointLocation[i] - FVector{0,0,GetTraceSize()};
			
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




FVector AEGProceduralPatrolGenerator::WorldToLocal(const FVector& WorldLocation) const
{
	return UKismetMathLibrary::InverseTransformLocation(GetActorTransform(), WorldLocation);
}

FVector AEGProceduralPatrolGenerator::LocalToWorld(const FVector& LocalLocation) const
{
	return UKismetMathLibrary::TransformLocation(GetActorTransform(), LocalLocation);
}


float AEGProceduralPatrolGenerator::GetTraceSize() const
{
	if (PatrolShape == EProceduralPatrolShape::Box)
		return BoxCollisionShape.Z * 1.5;
	
	if (PatrolShape == EProceduralPatrolShape::Sphere)
		return SphereRadius * 1.5;
	
	return 1000;
}


FVector AEGProceduralPatrolGenerator::GetRandomRelativeLocation() const
{
	if (PatrolShape == EProceduralPatrolShape::Box)
	{
		const FVector Location = UKismetMathLibrary::RandomPointInBoundingBox(GetActorLocation() , BoxCollisionShape);
		const FVector RelativeLocation =  UKismetMathLibrary::InverseTransformLocation(GetActorTransform(), Location);
		return RelativeLocation;
	}
	if (PatrolShape == EProceduralPatrolShape::Sphere)
	{
		const FVector Location = UEFMathLibrary::RandPointInSphere(GetActorLocation() , SphereRadius);
		const FVector RelativeLocation =  UKismetMathLibrary::InverseTransformLocation(GetActorTransform(), Location);
		return RelativeLocation;
	}
	return FVector{0,0,0};
}