// Fill out your copyright notice in the Description page of Project Settings.


#include "EFTraceLibrary.h"


bool UEFTraceLibrary::ExtendedLineTraceSingle(const UObject* WorldContextObject,FLineTraceStruct& LineTraceStruct)
{
	if (!WorldContextObject) return false;
	switch (LineTraceStruct.TraceType)
	{
		case TraceType:
			return UKismetSystemLibrary::LineTraceSingle(WorldContextObject->GetWorld(),
				LineTraceStruct.Start,
				LineTraceStruct.End,
				LineTraceStruct.TraceChannel,
				LineTraceStruct.bTraceComplex,
				LineTraceStruct.ActorsToIgnore,
				LineTraceStruct.DrawDebugType,
				LineTraceStruct.HitResult,
				LineTraceStruct.bIgnoreSelf,
				LineTraceStruct.TraceColor,
				LineTraceStruct.TraceHitColor,
				LineTraceStruct.DrawDebugType);
		case ProfileType:
			return UKismetSystemLibrary::LineTraceSingleByProfile(WorldContextObject->GetWorld(),
				LineTraceStruct.Start,
				LineTraceStruct.End,
				LineTraceStruct.TraceProfileName,
				LineTraceStruct.bTraceComplex,
				LineTraceStruct.ActorsToIgnore,
				LineTraceStruct.DrawDebugType,
				LineTraceStruct.HitResult,
				LineTraceStruct.bIgnoreSelf,
				LineTraceStruct.TraceColor,
				LineTraceStruct.TraceHitColor,
				LineTraceStruct.DrawDebugType);
			
		case ObjectsType:
			return UKismetSystemLibrary::LineTraceSingleForObjects(WorldContextObject->GetWorld(),
				LineTraceStruct.Start,
				LineTraceStruct.End,
				LineTraceStruct.TraceObjectTypes,
				LineTraceStruct.bTraceComplex,
				LineTraceStruct.ActorsToIgnore,
				LineTraceStruct.DrawDebugType,
				LineTraceStruct.HitResult,
				LineTraceStruct.bIgnoreSelf,
				LineTraceStruct.TraceColor,
				LineTraceStruct.TraceHitColor,
				LineTraceStruct.DrawDebugType);
		default: return false;
	}
}



bool UEFTraceLibrary::ExtendedLineTraceMulti(const UObject* WorldContextObject,FLineTraceStruct& LineTraceStruct)
{
	if (!WorldContextObject) return false;
	switch (LineTraceStruct.TraceType)
	{
	case TraceType:
		return UKismetSystemLibrary::LineTraceMulti(WorldContextObject->GetWorld(),
			LineTraceStruct.Start,
			LineTraceStruct.End,
			LineTraceStruct.TraceChannel,
			LineTraceStruct.bTraceComplex,
			LineTraceStruct.ActorsToIgnore,
			LineTraceStruct.DrawDebugType,
			LineTraceStruct.HitResults,
			LineTraceStruct.bIgnoreSelf,
			LineTraceStruct.TraceColor,
			LineTraceStruct.TraceHitColor,
			LineTraceStruct.DrawDebugType);
	case ProfileType:
		return UKismetSystemLibrary::LineTraceMultiByProfile(WorldContextObject->GetWorld(),
			LineTraceStruct.Start,
			LineTraceStruct.End,
			LineTraceStruct.TraceProfileName,
			LineTraceStruct.bTraceComplex,
			LineTraceStruct.ActorsToIgnore,
			LineTraceStruct.DrawDebugType,
			LineTraceStruct.HitResults,
			LineTraceStruct.bIgnoreSelf,
			LineTraceStruct.TraceColor,
			LineTraceStruct.TraceHitColor,
			LineTraceStruct.DrawDebugType);
			
	case ObjectsType:
		return UKismetSystemLibrary::LineTraceMultiForObjects(WorldContextObject->GetWorld(),
			LineTraceStruct.Start,
			LineTraceStruct.End,
			LineTraceStruct.TraceObjectTypes,
			LineTraceStruct.bTraceComplex,
			LineTraceStruct.ActorsToIgnore,
			LineTraceStruct.DrawDebugType,
			LineTraceStruct.HitResults,
			LineTraceStruct.bIgnoreSelf,
			LineTraceStruct.TraceColor,
			LineTraceStruct.TraceHitColor,
			LineTraceStruct.DrawDebugType);
	default: return false;
	}
	
}








bool UEFTraceLibrary::ExtendedBoxTraceSingle(const UObject* WorldContextObject, FBoxTraceStruct& BoxTraceStruct)
{
	if (!WorldContextObject) return false;
	switch (BoxTraceStruct.TraceType)
	{
	case TraceType:
		return UKismetSystemLibrary::BoxTraceSingle(WorldContextObject->GetWorld(),
			BoxTraceStruct.Start,
			BoxTraceStruct.End,
			BoxTraceStruct.HalfSize,
			BoxTraceStruct.Orientation,
			BoxTraceStruct.TraceChannel,
			BoxTraceStruct.bTraceComplex,
			BoxTraceStruct.ActorsToIgnore,
			BoxTraceStruct.DrawDebugType,
			BoxTraceStruct.HitResult,
			BoxTraceStruct.bIgnoreSelf,
			BoxTraceStruct.TraceColor,
			BoxTraceStruct.TraceHitColor,
			BoxTraceStruct.DrawDebugType);
	case ProfileType:
		return UKismetSystemLibrary::BoxTraceSingleByProfile(WorldContextObject->GetWorld(),
			BoxTraceStruct.Start,
			BoxTraceStruct.End,
			BoxTraceStruct.HalfSize,
			BoxTraceStruct.Orientation,
			BoxTraceStruct.TraceProfileName,
			BoxTraceStruct.bTraceComplex,
			BoxTraceStruct.ActorsToIgnore,
			BoxTraceStruct.DrawDebugType,
			BoxTraceStruct.HitResult,
			BoxTraceStruct.bIgnoreSelf,
			BoxTraceStruct.TraceColor,
			BoxTraceStruct.TraceHitColor,
			BoxTraceStruct.DrawDebugType);
			
	case ObjectsType:
		return UKismetSystemLibrary::BoxTraceSingleForObjects(WorldContextObject->GetWorld(),
			BoxTraceStruct.Start,
			BoxTraceStruct.End,
			BoxTraceStruct.HalfSize,
			BoxTraceStruct.Orientation,
			BoxTraceStruct.TraceObjectTypes,
			BoxTraceStruct.bTraceComplex,
			BoxTraceStruct.ActorsToIgnore,
			BoxTraceStruct.DrawDebugType,
			BoxTraceStruct.HitResult,
			BoxTraceStruct.bIgnoreSelf,
			BoxTraceStruct.TraceColor,
			BoxTraceStruct.TraceHitColor,
			BoxTraceStruct.DrawDebugType);
	default: return false;
	}

}



bool UEFTraceLibrary::ExtendedBoxTraceMulti(const UObject* WorldContextObject, FBoxTraceStruct& BoxTraceStruct)
{
	if (!WorldContextObject) return false;
	switch (BoxTraceStruct.TraceType)
	{
	case TraceType:
		return UKismetSystemLibrary::BoxTraceMulti(WorldContextObject->GetWorld(),
			BoxTraceStruct.Start,
			BoxTraceStruct.End,
			BoxTraceStruct.HalfSize,
			BoxTraceStruct.Orientation,
			BoxTraceStruct.TraceChannel,
			BoxTraceStruct.bTraceComplex,
			BoxTraceStruct.ActorsToIgnore,
			BoxTraceStruct.DrawDebugType,
			BoxTraceStruct.HitResults,
			BoxTraceStruct.bIgnoreSelf,
			BoxTraceStruct.TraceColor,
			BoxTraceStruct.TraceHitColor,
			BoxTraceStruct.DrawDebugType);
	case ProfileType:
		return UKismetSystemLibrary::BoxTraceMultiByProfile(WorldContextObject->GetWorld(),
			BoxTraceStruct.Start,
			BoxTraceStruct.End,
			BoxTraceStruct.HalfSize,
			BoxTraceStruct.Orientation,
			BoxTraceStruct.TraceProfileName,
			BoxTraceStruct.bTraceComplex,
			BoxTraceStruct.ActorsToIgnore,
			BoxTraceStruct.DrawDebugType,
			BoxTraceStruct.HitResults,
			BoxTraceStruct.bIgnoreSelf,
			BoxTraceStruct.TraceColor,
			BoxTraceStruct.TraceHitColor,
			BoxTraceStruct.DrawDebugType);
			
	case ObjectsType:
		return UKismetSystemLibrary::BoxTraceMultiForObjects(WorldContextObject->GetWorld(),
			BoxTraceStruct.Start,
			BoxTraceStruct.End,
			BoxTraceStruct.HalfSize,
			BoxTraceStruct.Orientation,
			BoxTraceStruct.TraceObjectTypes,
			BoxTraceStruct.bTraceComplex,
			BoxTraceStruct.ActorsToIgnore,
			BoxTraceStruct.DrawDebugType,
			BoxTraceStruct.HitResults,
			BoxTraceStruct.bIgnoreSelf,
			BoxTraceStruct.TraceColor,
			BoxTraceStruct.TraceHitColor,
			BoxTraceStruct.DrawDebugType);
	default: return false;
	}
}








bool UEFTraceLibrary::ExtendedSphereTraceSingle(const UObject* WorldContextObject,FSphereTraceStruct& SphereTraceStruct)
{
	if (!WorldContextObject) return false;
	switch (SphereTraceStruct.TraceType)
	{
	case TraceType:
		return UKismetSystemLibrary::SphereTraceSingle(WorldContextObject->GetWorld(),
			SphereTraceStruct.Start,
			SphereTraceStruct.End,
			SphereTraceStruct.Radius,
			SphereTraceStruct.TraceChannel,
			SphereTraceStruct.bTraceComplex,
			SphereTraceStruct.ActorsToIgnore,
			SphereTraceStruct.DrawDebugType,
			SphereTraceStruct.HitResult,
			SphereTraceStruct.bIgnoreSelf,
			SphereTraceStruct.TraceColor,
			SphereTraceStruct.TraceHitColor,
			SphereTraceStruct.DrawDebugType);
	case ProfileType:
		return UKismetSystemLibrary::SphereTraceSingleByProfile(WorldContextObject->GetWorld(),
			SphereTraceStruct.Start,
			SphereTraceStruct.End,
			SphereTraceStruct.Radius,
			SphereTraceStruct.TraceProfileName,
			SphereTraceStruct.bTraceComplex,
			SphereTraceStruct.ActorsToIgnore,
			SphereTraceStruct.DrawDebugType,
			SphereTraceStruct.HitResult,
			SphereTraceStruct.bIgnoreSelf,
			SphereTraceStruct.TraceColor,
			SphereTraceStruct.TraceHitColor,
			SphereTraceStruct.DrawDebugType);
			
	case ObjectsType:
		return UKismetSystemLibrary::SphereTraceSingleForObjects(WorldContextObject->GetWorld(),
			SphereTraceStruct.Start,
			SphereTraceStruct.End,
			SphereTraceStruct.Radius,
			SphereTraceStruct.TraceObjectTypes,
			SphereTraceStruct.bTraceComplex,
			SphereTraceStruct.ActorsToIgnore,
			SphereTraceStruct.DrawDebugType,
			SphereTraceStruct.HitResult,
			SphereTraceStruct.bIgnoreSelf,
			SphereTraceStruct.TraceColor,
			SphereTraceStruct.TraceHitColor,
			SphereTraceStruct.DrawDebugType);
	default: return false;
	}
}



bool UEFTraceLibrary::ExtendedSphereTraceMulti(const UObject* WorldContextObject,FSphereTraceStruct& SphereTraceStruct)
{

	if (!WorldContextObject) return false;
	switch (SphereTraceStruct.TraceType)
	{
	case TraceType:
		return UKismetSystemLibrary::SphereTraceMulti(WorldContextObject->GetWorld(),
			SphereTraceStruct.Start,
			SphereTraceStruct.End,
			SphereTraceStruct.Radius,
			SphereTraceStruct.TraceChannel,
			SphereTraceStruct.bTraceComplex,
			SphereTraceStruct.ActorsToIgnore,
			SphereTraceStruct.DrawDebugType,
			SphereTraceStruct.HitResults,
			SphereTraceStruct.bIgnoreSelf,
			SphereTraceStruct.TraceColor,
			SphereTraceStruct.TraceHitColor,
			SphereTraceStruct.DrawDebugType);
	case ProfileType:
		return UKismetSystemLibrary::SphereTraceMultiByProfile(WorldContextObject->GetWorld(),
			SphereTraceStruct.Start,
			SphereTraceStruct.End,
			SphereTraceStruct.Radius,
			SphereTraceStruct.TraceProfileName,
			SphereTraceStruct.bTraceComplex,
			SphereTraceStruct.ActorsToIgnore,
			SphereTraceStruct.DrawDebugType,
			SphereTraceStruct.HitResults,
			SphereTraceStruct.bIgnoreSelf,
			SphereTraceStruct.TraceColor,
			SphereTraceStruct.TraceHitColor,
			SphereTraceStruct.DrawDebugType);
			
	case ObjectsType:
		return UKismetSystemLibrary::SphereTraceMultiForObjects(WorldContextObject->GetWorld(),
			SphereTraceStruct.Start,
			SphereTraceStruct.End,
			SphereTraceStruct.Radius,
			SphereTraceStruct.TraceObjectTypes,
			SphereTraceStruct.bTraceComplex,
			SphereTraceStruct.ActorsToIgnore,
			SphereTraceStruct.DrawDebugType,
			SphereTraceStruct.HitResults,
			SphereTraceStruct.bIgnoreSelf,
			SphereTraceStruct.TraceColor,
			SphereTraceStruct.TraceHitColor,
			SphereTraceStruct.DrawDebugType);
	default: return false;
	}
}








bool UEFTraceLibrary::ExtendedCapsuleTraceSingle(const UObject* WorldContextObject,FCapsuleTraceStruct& CapsuleTraceStruct)
{
	if (!WorldContextObject) return false;
	switch (CapsuleTraceStruct.TraceType)
	{
	case TraceType:
		return UKismetSystemLibrary::CapsuleTraceSingle(WorldContextObject->GetWorld(),
			CapsuleTraceStruct.Start,
			CapsuleTraceStruct.End,
			CapsuleTraceStruct.Radius,
			CapsuleTraceStruct.HalfHeight,
			CapsuleTraceStruct.TraceChannel,
			CapsuleTraceStruct.bTraceComplex,
			CapsuleTraceStruct.ActorsToIgnore,
			CapsuleTraceStruct.DrawDebugType,
			CapsuleTraceStruct.HitResult,
			CapsuleTraceStruct.bIgnoreSelf,
			CapsuleTraceStruct.TraceColor,
			CapsuleTraceStruct.TraceHitColor,
			CapsuleTraceStruct.DrawDebugType);
	case ProfileType:
		return UKismetSystemLibrary::CapsuleTraceSingleByProfile(WorldContextObject->GetWorld(),
			CapsuleTraceStruct.Start,
			CapsuleTraceStruct.End,
			CapsuleTraceStruct.Radius,
			CapsuleTraceStruct.HalfHeight,
			CapsuleTraceStruct.TraceProfileName,
			CapsuleTraceStruct.bTraceComplex,
			CapsuleTraceStruct.ActorsToIgnore,
			CapsuleTraceStruct.DrawDebugType,
			CapsuleTraceStruct.HitResult,
			CapsuleTraceStruct.bIgnoreSelf,
			CapsuleTraceStruct.TraceColor,
			CapsuleTraceStruct.TraceHitColor,
			CapsuleTraceStruct.DrawDebugType);
			
	case ObjectsType:
		return UKismetSystemLibrary::CapsuleTraceSingleForObjects(WorldContextObject->GetWorld(),
			CapsuleTraceStruct.Start,
			CapsuleTraceStruct.End,
			CapsuleTraceStruct.Radius,
			CapsuleTraceStruct.HalfHeight,
			CapsuleTraceStruct.TraceObjectTypes,
			CapsuleTraceStruct.bTraceComplex,
			CapsuleTraceStruct.ActorsToIgnore,
			CapsuleTraceStruct.DrawDebugType,
			CapsuleTraceStruct.HitResult,
			CapsuleTraceStruct.bIgnoreSelf,
			CapsuleTraceStruct.TraceColor,
			CapsuleTraceStruct.TraceHitColor,
			CapsuleTraceStruct.DrawDebugType);
	default: return false;
	}
}



bool UEFTraceLibrary::ExtendedCapsuleTraceMulti(const UObject* WorldContextObject,FCapsuleTraceStruct& CapsuleTraceStruct)
{
	if (!WorldContextObject) return false;
	switch (CapsuleTraceStruct.TraceType)
	{
	case TraceType:
		return UKismetSystemLibrary::CapsuleTraceMulti(WorldContextObject->GetWorld(),
			CapsuleTraceStruct.Start,
			CapsuleTraceStruct.End,
			CapsuleTraceStruct.Radius,
			CapsuleTraceStruct.HalfHeight,
			CapsuleTraceStruct.TraceChannel,
			CapsuleTraceStruct.bTraceComplex,
			CapsuleTraceStruct.ActorsToIgnore,
			CapsuleTraceStruct.DrawDebugType,
			CapsuleTraceStruct.HitResults,
			CapsuleTraceStruct.bIgnoreSelf,
			CapsuleTraceStruct.TraceColor,
			CapsuleTraceStruct.TraceHitColor,
			CapsuleTraceStruct.DrawDebugType);
	case ProfileType:
		return UKismetSystemLibrary::CapsuleTraceMultiByProfile(WorldContextObject->GetWorld(),
			CapsuleTraceStruct.Start,
			CapsuleTraceStruct.End,
			CapsuleTraceStruct.Radius,
			CapsuleTraceStruct.HalfHeight,
			CapsuleTraceStruct.TraceProfileName,
			CapsuleTraceStruct.bTraceComplex,
			CapsuleTraceStruct.ActorsToIgnore,
			CapsuleTraceStruct.DrawDebugType,
			CapsuleTraceStruct.HitResults,
			CapsuleTraceStruct.bIgnoreSelf,
			CapsuleTraceStruct.TraceColor,
			CapsuleTraceStruct.TraceHitColor,
			CapsuleTraceStruct.DrawDebugType);
			
	case ObjectsType:
		return UKismetSystemLibrary::CapsuleTraceMultiForObjects(WorldContextObject->GetWorld(),
			CapsuleTraceStruct.Start,
			CapsuleTraceStruct.End,
			CapsuleTraceStruct.Radius,
			CapsuleTraceStruct.HalfHeight,
			CapsuleTraceStruct.TraceObjectTypes,
			CapsuleTraceStruct.bTraceComplex,
			CapsuleTraceStruct.ActorsToIgnore,
			CapsuleTraceStruct.DrawDebugType,
			CapsuleTraceStruct.HitResults,
			CapsuleTraceStruct.bIgnoreSelf,
			CapsuleTraceStruct.TraceColor,
			CapsuleTraceStruct.TraceHitColor,
			CapsuleTraceStruct.DrawDebugType);
	default: return false;
	}
}
