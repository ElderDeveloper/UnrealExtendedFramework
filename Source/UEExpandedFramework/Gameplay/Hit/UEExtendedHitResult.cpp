// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedHitResult.h"


FVector UUEExtendedHitResult::GetHitLocationFromHitResult(FHitResult HitResult)
{
	return HitResult.Location;
}

FVector UUEExtendedHitResult::GetTraceEndFromHitResult(FHitResult HitResult)
{
	return HitResult.TraceEnd;
}

AActor* UUEExtendedHitResult::GetHitActorFromHitResult(FHitResult HitResult)
{
	return HitResult.Actor.Get();
}

USceneComponent* UUEExtendedHitResult::GetHitComponentFromHitResult(FHitResult HitResult)
{
	return HitResult.GetComponent();
}

