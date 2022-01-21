// Fill out your copyright notice in the Description page of Project Settings.


#include "EFEQSGeneratorDirection.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"


void UEFEQSGeneratorDirection::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	//This array will hold a reference to all the generated items, meaning, the cone items
	TArray<FNavLocation> ItemCandidates;
 
	//Get a reference for our AI Pawn
	const AActor* AIPawn = Cast<AActor>((QueryInstance.Owner).Get());

	TArray<AActor*> CenterActors;
	QueryInstance.PrepareContext(CenterDirectionActor, CenterActors);

	if (CenterActors.Num() <= 0)
	{
		return;
	}

	const auto TargetActor = CenterActors[0];
 
	//Store its location and its forward vector
	const FVector PawnLocation = BaseLocationIsContext ? TargetActor->GetActorLocation() : AIPawn->GetActorLocation();
	const FVector PawnForwardVector =BaseLocationIsContext? UEFMathLibrary::GetDirectionBetweenActors(TargetActor , AIPawn) :  UEFMathLibrary::GetDirectionBetweenActors(AIPawn , TargetActor);
 
	//If the angle step is zero we're going into an infinite loop. 
	//Since we don't want that, don't execute the following logic
	if (ExtendedAngleStep == 0) return;


	
 
	for (float Angle = -ExtendedConeDegrees; Angle < ExtendedConeDegrees; Angle += ExtendedAngleStep)
	{
		//Start from the left side of the pawn and rotate its forward vector by Angle + 1
		FVector LeftVector = PawnForwardVector.RotateAngleAxis(Angle + 1, FVector(0, 0, 1));
		//The Left Vector is showing a straight line for that angle. The only thing we need
		//is to generate items in that line
 
		//Generates all the points for the current line (LeftVector)
		for (int32 Point = 0; Point * ExtendedPointsDistance < ExtendedConeRadius; Point++)
		{
			//Generate a point for this particular angle and distance
			FNavLocation NavLoc = FNavLocation(PawnLocation + LeftVector * Point * ExtendedPointsDistance);
 
			//Add the new point into our array
			ItemCandidates.Add(NavLoc);
		}
	}
 
	//Projects all the nav points into our Viewport and removes those outside of our navmesh
	ProjectAndFilterNavPoints(ItemCandidates, QueryInstance);
 
	//Store the generated points as the result of our Query
	StoreNavPoints(ItemCandidates, QueryInstance);
	
}


FText UEFEQSGeneratorDirection::GetDescriptionTitle() const
{
	FString S = "Points: Cone Direction";
	return FText::FromString(S);
}

FText UEFEQSGeneratorDirection::GetDescriptionDetails() const
{
	FString S = "Points: Cone Direction";
	return FText::FromString(S);
	/*
	FText Desc = FText::Format(TEXT("degrees: {0}, angle step: {1}"),
		FText::FromString(ConeDegrees.ToString()), FText::FromString(AngleStep.ToString()));

	FText ProjDesc = ProjectionData.ToText(FEnvTraceData::Brief);
	if (!ProjDesc.IsEmpty())
	{
		FFormatNamedArguments ProjArgs;
		ProjArgs.Add(TEXT("Description"), Desc);
		ProjArgs.Add(TEXT("ProjectionDescription"), ProjDesc);
		Desc = FText::Format(LOCTEXT("ConeDescriptionWithProjection", "{Description}, {ProjectionDescription}"), ProjArgs);
	}

	return Desc;
	*/
}