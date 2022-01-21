// Fill out your copyright notice in the Description page of Project Settings.


#include "EFHitResultLibrary.h"



FVector UEFHitResultLibrary::GetHitLocationFromHitResult(FHitResult HitResult)
{
	return HitResult.Location;
}

FVector UEFHitResultLibrary::GetTraceEndFromHitResult(FHitResult HitResult)
{
	return HitResult.TraceEnd;
}

AActor* UEFHitResultLibrary::GetHitActorFromHitResult(FHitResult HitResult)
{
	return HitResult.GetActor();
}

USceneComponent* UEFHitResultLibrary::GetHitComponentFromHitResult(FHitResult HitResult)
{
	return HitResult.GetComponent();
}

float UEFHitResultLibrary::GetHitDistance(FHitResult HitResult)
{
	return FMath::Sqrt(
	HitResult.TraceStart.X	* HitResult.Location.X +
		HitResult.TraceStart.Y * HitResult.Location.Y +
		HitResult.TraceStart.Z * HitResult.Location.Z);
}