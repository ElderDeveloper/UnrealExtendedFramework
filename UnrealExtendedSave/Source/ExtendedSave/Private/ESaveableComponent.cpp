// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESaveableComponent.h"

#include "ESaveSubsystem.h"
#include "GameFramework/Actor.h"
#include "Misc/Guid.h"

UESaveableComponent::UESaveableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UESaveableComponent::IsPlacedInLevel() const
{
	// Set for every actor that came with its level, before BeginPlay; never for spawned ones.
	const AActor* Owner = GetOwner();
	return Owner && Owner->IsNetStartupActor();
}

FName UESaveableComponent::GetResolvedSaveId()
{
	if (!SaveId.IsNone())
	{
		return SaveId;
	}
	if (!CachedSaveId.IsNone())
	{
		return CachedSaveId;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return NAME_None;
	}

	if (IsPlacedInLevel())
	{
		CachedSaveId = FName(*FString::Printf(TEXT("%s:%s"), *UESaveSubsystem::GetLevelKey(Owner), *Owner->GetName()));
	}
	else
	{
		if (RuntimeSaveId.IsNone())
		{
			RuntimeSaveId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		}
		CachedSaveId = RuntimeSaveId;
	}
	return CachedSaveId;
}

void UESaveableComponent::SetRuntimeSaveId(FName InId)
{
	RuntimeSaveId = InId;
	CachedSaveId = NAME_None;
}

void UESaveableComponent::OnRegister()
{
	Super::OnRegister();
	// No game instance in editor worlds: the lookup returns null and nothing is registered.
	if (UESaveSubsystem* Save = UESaveSubsystem::Get(this))
	{
		Save->RegisterComponent(this);
	}
}

void UESaveableComponent::OnUnregister()
{
	if (UESaveSubsystem* Save = UESaveSubsystem::Get(this))
	{
		Save->UnregisterComponent(this);
	}
	Super::OnUnregister();
}

void UESaveableComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UESaveSubsystem* Save = UESaveSubsystem::Get(this))
	{
		Save->RegisterComponent(this);
		Save->ApplyToComponent(this);
	}
}

void UESaveableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UESaveSubsystem* Save = UESaveSubsystem::Get(this))
	{
		switch (EndPlayReason)
		{
		case EEndPlayReason::Destroyed:
			Save->NotifyComponentDestroyed(this);
			break;
		case EEndPlayReason::RemovedFromWorld:
		case EEndPlayReason::LevelTransition:
			Save->NotifyComponentUnloaded(this);
			break;
		default:
			// Quitting or leaving PIE: nothing to keep.
			Save->UnregisterComponent(this);
			break;
		}
	}
	Super::EndPlay(EndPlayReason);
}
