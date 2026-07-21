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

UEFModularSettingsBase* UEFWorldSettingsComponent::AddSettingFromTemplate_Local(UEFModularSettingsBase* Template)
{
	if (!Template)
	{
		return nullptr;
	}

	if (!Template->SettingTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEFWorldSettingsComponent] Template has invalid SettingTag. Outer=%s"), *GetNameSafe(Template->GetOuter()));
		return nullptr;
	}

	if (UEFModularSettingsBase* ExistingSetting = GetSettingByTag(Template->SettingTag))
	{
		return ExistingSetting;
	}

	UEFModularSettingsBase* NewSetting = DuplicateObject<UEFModularSettingsBase>(Template, this);
	if (!NewSetting)
	{
		return nullptr;
	}

	Settings.Add(NewSetting);

	if (GetOwnerRole() == ROLE_Authority)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->AddReplicatedSubObject(NewSetting);
		}
	}

	return NewSetting;
}

void UEFWorldSettingsComponent::BeginPlay()
{
	Super::BeginPlay();

	auto ConsumeTemplate = [this](UEFModularSettingsBase* Template)
	{
		if (!Template || !Template->SettingTag.IsValid())
		{
			return;
		}

		bool bExists = false;
		for (const FEFWorldSettingDefinition& Def : SettingDefinitions)
		{
			if (Def.Tag == Template->SettingTag)
			{
				bExists = true;
				break;
			}
		}

		if (!bExists)
		{
			FEFWorldSettingDefinition NewDef;
			NewDef.Tag = Template->SettingTag;
			NewDef.Template = Template;
			NewDef.CurrentValue = Template->GetValueAsString();
			SettingDefinitions.Add(NewDef);
		}

		AddSettingFromTemplate_Local(Template);
	};

	// Only create settings from templates on the server.
	// Clients receive the current world setting set through replicated definitions
	// and replicated subobjects; creating local defaults here causes late joiners
	// to read stale default values before the replicated state arrives.
	if (GetOwnerRole() == ROLE_Authority)
	{
		if (const UEFModularProjectSettings* ProjectSettings = GetDefault<UEFModularProjectSettings>())
			{
				for (const TSoftObjectPtr<UEFModularSettingsContainer>& ContainerPtr : ProjectSettings->WorldSettingsContainers)
				{
					if (UEFModularSettingsContainer* Container = ContainerPtr.LoadSynchronous())
					{
						for (UEFModularSettingsBase* Template : Container->Settings)
						{
							ConsumeTemplate(Template);
						}
					}
				}
			}

		for (UEFModularSettingsBase* DefaultSetting : DefaultSettings)
		{
			ConsumeTemplate(DefaultSetting);
		}
	}
}


void UEFWorldSettingsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEFWorldSettingsComponent, Settings);
	DOREPLIFETIME(UEFWorldSettingsComponent, SettingDefinitions);
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
		UpdateDefinitionCurrentValue(Tag, Setting->GetValueAsString());
		OnSettingChanged.Broadcast(Setting);
	}
}


void UEFWorldSettingsComponent::OnRep_Settings()
{
	for (auto Setting : Settings)
	{
		if (Setting)
		{
			OnSettingChanged.Broadcast(Setting);
		}
	}
}

void UEFWorldSettingsComponent::OnRep_SettingDefinitions()
{
	for (const FEFWorldSettingDefinition& Def : SettingDefinitions)
	{
		if (!Def.Tag.IsValid() || GetSettingByTag(Def.Tag))
		{
			continue;
		}

		UEFModularSettingsBase* Template = Def.Template.LoadSynchronous();
		if (!Template)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UEFWorldSettingsComponent] Failed to load template for tag %s"), *Def.Tag.ToString());
			continue;
		}

		if (Template->SettingTag != Def.Tag)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UEFWorldSettingsComponent] Template tag mismatch. Def=%s Template=%s"), *Def.Tag.ToString(), *Template->SettingTag.ToString());
		}

		AddSettingFromTemplate_Local(Template);
	}

	for (const FEFWorldSettingDefinition& Def : SettingDefinitions)
	{
		if (!Def.Tag.IsValid())
		{
			continue;
		}

		if (UEFModularSettingsBase* Setting = GetSettingByTag(Def.Tag))
		{
			if (!Def.CurrentValue.IsEmpty() && Setting->GetValueAsString() != Def.CurrentValue)
			{
				Setting->SetValueFromString(Def.CurrentValue);
				Setting->Apply();
			}

			OnSettingChanged.Broadcast(Setting);
		}
	}
}

void UEFWorldSettingsComponent::UpdateDefinitionCurrentValue(FGameplayTag Tag, const FString& NewValue)
{
	for (FEFWorldSettingDefinition& Def : SettingDefinitions)
	{
		if (Def.Tag == Tag)
		{
			Def.CurrentValue = NewValue;
			return;
		}
	}
}
