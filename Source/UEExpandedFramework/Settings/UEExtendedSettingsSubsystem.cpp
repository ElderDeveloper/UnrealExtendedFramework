// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedSettingsSubsystem.h"


bool UUEExtendedSettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

void UUEExtendedSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}
