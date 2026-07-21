// Fill out your copyright notice in the Description page of Project Settings.


#include "EGSenseConfig_Faction.h"

UEGSenseConfig_Faction::UEGSenseConfig_Faction(const FObjectInitializer& ObjectInitializer)
{
	DebugColor = FColor::Blue;
	Implementation = *Implementation;
}

TSubclassOf<UAISense> UEGSenseConfig_Faction::GetSenseImplementation() const
{
	return *Implementation;
}
