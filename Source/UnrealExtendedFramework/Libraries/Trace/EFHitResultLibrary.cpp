// Fill out your copyright notice in the Description page of Project Settings.


#include "EFHitResultLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


FVector UEFHitResultLibrary::GetHitLocationFromHitResult(const FHitResult& HitResult)
{
	return HitResult.Location;
}

FVector UEFHitResultLibrary::GetTraceEndFromHitResult(const FHitResult& HitResult)
{
	return HitResult.TraceEnd;
}

FVector UEFHitResultLibrary::GetTraceStartFromHitResult(const FHitResult& HitResult)
{
	return HitResult.TraceStart;
}

AActor* UEFHitResultLibrary::GetHitActorFromHitResult(const FHitResult& HitResult)
{
	return HitResult.GetActor();
}

USceneComponent* UEFHitResultLibrary::GetHitComponentFromHitResult(const FHitResult& HitResult)
{
	return HitResult.GetComponent();
}

float UEFHitResultLibrary::GetHitDistance(const FHitResult& HitResult)
{
	return FVector::Dist(HitResult.TraceStart, HitResult.Location);
}

FVector UEFHitResultLibrary::GetHitImpactNormal(const FHitResult& HitResult)
{
	return HitResult.ImpactNormal;
}

FVector UEFHitResultLibrary::GetHitImpactPoint(const FHitResult& HitResult)
{
	return HitResult.ImpactPoint;
}

FVector UEFHitResultLibrary::GetHitNormal(const FHitResult& HitResult)
{
	return HitResult.Normal;
}

bool UEFHitResultLibrary::IsBlockingHit(const FHitResult& HitResult)
{
	return HitResult.bBlockingHit;
}

FName UEFHitResultLibrary::GetHitBoneName(const FHitResult& HitResult)
{
	return HitResult.BoneName;
}

UPhysicalMaterial* UEFHitResultLibrary::GetHitPhysMaterial(const FHitResult& HitResult)
{
	return HitResult.PhysMaterial.Get();
}