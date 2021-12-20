// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedHitResultLibrary.h"


FVector UUEExtendedHitResultLibrary::GetHitLocationFromHitResult(FHitResult HitResult)
{
	return HitResult.Location;
}

FVector UUEExtendedHitResultLibrary::GetTraceEndFromHitResult(FHitResult HitResult)
{
	return HitResult.TraceEnd;
}

AActor* UUEExtendedHitResultLibrary::GetHitActorFromHitResult(FHitResult HitResult)
{
	return HitResult.GetActor();
}

USceneComponent* UUEExtendedHitResultLibrary::GetHitComponentFromHitResult(FHitResult HitResult)
{
	return HitResult.GetComponent();
}

float UUEExtendedHitResultLibrary::GetHitDistance(FHitResult HitResult)
{
	return FMath::Sqrt(
	HitResult.TraceStart.X	* HitResult.Location.X +
		HitResult.TraceStart.Y * HitResult.Location.Y +
		HitResult.TraceStart.Z * HitResult.Location.Z);
}