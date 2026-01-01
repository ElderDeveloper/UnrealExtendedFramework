// Fill out your copyright notice in the Description page of Project Settings.


#include "EFWorldSettingsComponent.h"

#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"

UEFWorldSettingsComponent::UEFWorldSettingsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEFWorldSettingsComponent::BeginPlay()
{
	Super::BeginPlay();

	// Create settings on BOTH server and client
	// The object structures are the same (from templates), only VALUES replicate
	auto AddSettingLambda = [this](const UEFModularSettingsBase* Setting)
	{
		if (Setting && Setting->SettingTag.IsValid())
		{
			if (!GetSettingByTag(Setting->SettingTag))
			{
				UEFModularSettingsBase* NewSetting = DuplicateObject<UEFModularSettingsBase>(Setting, this);
				Settings.Add(NewSetting);
				
				// Only register as replicated subobject on server
				if (GetOwnerRole() == ROLE_Authority)
				{
					if (AActor* Owner = GetOwner())
					{
						Owner->AddReplicatedSubObject(NewSetting);
					}
				}
			}
		}
	};

	if (const UEFModularProjectSettings* ProjectSettings = GetDefault<UEFModularProjectSettings>())
	{
		for (const TSoftObjectPtr<UEFModularSettingsContainer>& ContainerPtr : ProjectSettings->WorldSettingsContainers)
		{
			if (UEFModularSettingsContainer* Container = ContainerPtr.LoadSynchronous())
			{
				for (UEFModularSettingsBase* Setting : Container->Settings)
				{
					AddSettingLambda(Setting);
				}
			}
		}
	}

	// Also initialize from local defaults (if any)
	for (auto DefaultSetting : DefaultSettings)
	{
		AddSettingLambda(DefaultSetting);
	}
}


void UEFWorldSettingsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEFWorldSettingsComponent, Settings);
}


bool UEFWorldSettingsComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (UEFModularSettingsBase* Setting : Settings)
	{
		if (Setting)
		{
			bWroteSomething |= Channel->ReplicateSubobject(Setting, *Bunch, *RepFlags);
		}
	}

	return bWroteSomething;
}


UEFModularSettingsBase* UEFWorldSettingsComponent::GetSettingByTag(FGameplayTag Tag) const
{
	for (auto Setting : Settings)
	{
		if (Setting && Setting->SettingTag == Tag)
		{
			return Setting;
		}
	}
	return nullptr;
}


TArray<UEFModularSettingsBase*> UEFWorldSettingsComponent::GetSettingsByCategory(FName Category) const
{
	TArray<UEFModularSettingsBase*> FoundSettings;
	for (auto Setting : Settings)
	{
		if (Setting && Setting->ConfigCategory == Category)
		{
			FoundSettings.Add(Setting);
		}
	}
	return FoundSettings;
}


int32 UEFWorldSettingsComponent::FindSettingIndex(FGameplayTag Tag) const
{
	for (int32 i = 0; i < Settings.Num(); ++i)
	{
		if (Settings[i] && Settings[i]->SettingTag == Tag)
		{
			return i;
		}
	}
	return INDEX_NONE;
}


void UEFWorldSettingsComponent::UpdateSettingValue(FGameplayTag Tag, const FString& NewValue)
{
	if (GetOwnerRole() != ROLE_Authority) return;

	if (UEFModularSettingsBase* Setting = GetSettingByTag(Tag))
	{
		Setting->SetValueFromString(NewValue);
		Setting->Apply();
		OnSettingChanged.Broadcast(Setting);
	}
}


void UEFWorldSettingsComponent::OnRep_Settings()
{
	// Optional: Broadcast changes for UI updates
	for (auto Setting : Settings)
	{
		if (Setting)
		{
			OnSettingChanged.Broadcast(Setting);
		}
	}
}
