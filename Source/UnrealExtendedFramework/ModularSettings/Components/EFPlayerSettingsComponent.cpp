// Fill out your copyright notice in the Description page of Project Settings.


#include "EFPlayerSettingsComponent.h"

#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"

UEFPlayerSettingsComponent::UEFPlayerSettingsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UEFModularSettingsBase* UEFPlayerSettingsComponent::AddSettingFromTemplate_Local(UEFModularSettingsBase* Template)
{
	if (!Template)
	{
		return nullptr;
	}

	if (!Template->SettingTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent] Template has invalid SettingTag. Outer=%s"), *GetNameSafe(Template->GetOuter()));
		return nullptr;
	}

	if (UEFModularSettingsBase* ExistingSetting = GetSettingByTag(Template->SettingTag))
	{
		// Already exists
		return ExistingSetting;
	}

	UEFModularSettingsBase* NewSetting = DuplicateObject<UEFModularSettingsBase>(Template, this);
	if (!NewSetting)
	{
		return nullptr;
	}

	Settings.Add(NewSetting);

	// Only register as replicated subobject on server
	if (GetOwnerRole() == ROLE_Authority)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->AddReplicatedSubObject(NewSetting);
		}
	}

	OnSettingChanged.Broadcast(NewSetting);
	return NewSetting;
}

void UEFPlayerSettingsComponent::BeginPlay()
{
	Super::BeginPlay();
	
	auto ConsumeTemplate = [this](UEFModularSettingsBase* Template)
	{
		if (!Template || !Template->SettingTag.IsValid())
		{
			return;
		}

		// Track the definition on the server so clients/late-joiners also create the object.
		if (GetOwnerRole() == ROLE_Authority)
		{
			bool bExists = false;
			for (const FEFPlayerSettingDefinition& Def : SettingDefinitions)
			{
				if (Def.Tag == Template->SettingTag)
				{
					bExists = true;
					break;
				}
			}

			if (!bExists)
			{
				FEFPlayerSettingDefinition NewDef;
				NewDef.Tag = Template->SettingTag;
				NewDef.Template = Template;
				SettingDefinitions.Add(NewDef);
			}
		}

		AddSettingFromTemplate_Local(Template);
	};

	// Project containers
	if (const UEFModularProjectSettings* ProjectSettings = GetDefault<UEFModularProjectSettings>())
	{
		for (const TSoftObjectPtr<UEFModularSettingsContainer>& ContainerPtr : ProjectSettings->PlayerSettingsContainers)
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
	
	for (UEFModularSettingsBase* Template : DefaultSettings)
	{
		ConsumeTemplate(Template);
	}
}

void UEFPlayerSettingsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEFPlayerSettingsComponent, Settings);
	DOREPLIFETIME(UEFPlayerSettingsComponent, SettingDefinitions);
	DOREPLIFETIME(UEFPlayerSettingsComponent, RuntimeSettingDefinitions);
}

bool UEFPlayerSettingsComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
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

UEFModularSettingsBase* UEFPlayerSettingsComponent::GetSettingByTag(FGameplayTag Tag) const
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

TArray<UEFModularSettingsBase*> UEFPlayerSettingsComponent::GetSettingsByCategory(FName Category) const
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


void UEFPlayerSettingsComponent::RequestUpdateSetting(FGameplayTag Tag, const FString& NewValue)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		ServerUpdateSetting_Implementation(Tag, NewValue);
	}
	else
	{
		ServerUpdateSetting(Tag, NewValue);
	}
}

bool UEFPlayerSettingsComponent::ServerUpdateSetting_Validate(FGameplayTag Tag, const FString& NewValue)
{
	// Reject obviously invalid tags; value validation is done against the setting.
	return Tag.IsValid();
}

void UEFPlayerSettingsComponent::ServerUpdateSetting_Implementation(FGameplayTag Tag, const FString& NewValue)
{
	if (UEFModularSettingsBase* Setting = GetSettingByTag(Tag))
	{
		if (Setting->Validate(NewValue))
		{
			Setting->SetValueFromString(NewValue);
			Setting->Apply();
			OnSettingChanged.Broadcast(Setting);
		}
	}
}

void UEFPlayerSettingsComponent::OnRep_Settings()
{
	for (auto Setting : Settings)
	{
		if (Setting)
		{
			OnSettingChanged.Broadcast(Setting);
		}
	}
}

void UEFPlayerSettingsComponent::OnRep_SettingDefinitions()
{
	// Build any missing settings from replicated definitions.
	for (const FEFPlayerSettingDefinition& Def : SettingDefinitions)
	{
		if (!Def.Tag.IsValid())
		{
			continue;
		}

		if (GetSettingByTag(Def.Tag))
		{
			continue;
		}

		UEFModularSettingsBase* Template = Def.Template.LoadSynchronous();
		if (!Template)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent] Failed to load template for tag %s"), *Def.Tag.ToString());
			continue;
		}

		// Safety: ensure the loaded template tag matches the replicated tag.
		if (Template->SettingTag != Def.Tag)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent] Template tag mismatch. Def=%s Template=%s"), *Def.Tag.ToString(), *Template->SettingTag.ToString());
		}

		AddSettingFromTemplate_Local(Template);
	}
}

UEFModularSettingsBool* UEFPlayerSettingsComponent::AddBoolSetting(
	FGameplayTag Tag,
	FText DisplayName,
	FName ConfigCategory,
	bool DefaultValue,
	bool InitialValue)
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent::AddBoolSetting] Invalid tag provided."));
		return nullptr;
	}

	// Check if setting already exists
	if (UEFModularSettingsBool* Existing = Cast<UEFModularSettingsBool>(GetSettingByTag(Tag)))
	{
		return Existing;
	}

	// Only server can create new settings
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent::AddBoolSetting] Only server can create settings."));
		return nullptr;
	}

	// Create the runtime definition that will be replicated to clients
	FEFRuntimeSettingDefinition Def;
	Def.Tag = Tag;
	Def.DisplayName = DisplayName;
	Def.ConfigCategory = ConfigCategory;
	Def.SettingType = 0; // Bool
	Def.BoolDefaultValue = DefaultValue;
	Def.BoolValue = InitialValue;
	RuntimeSettingDefinitions.Add(Def);

	// Create the setting locally on server
	UEFModularSettingsBool* NewSetting = Cast<UEFModularSettingsBool>(CreateSettingFromRuntimeDefinition(Def));
	
	return NewSetting;
}

UEFModularSettingsFloat* UEFPlayerSettingsComponent::AddFloatSetting(
	FGameplayTag Tag,
	FText DisplayName,
	FName ConfigCategory,
	float DefaultValue,
	float InitialValue,
	float Min,
	float Max)
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent::AddFloatSetting] Invalid tag provided."));
		return nullptr;
	}

	// Check if setting already exists
	if (UEFModularSettingsFloat* Existing = Cast<UEFModularSettingsFloat>(GetSettingByTag(Tag)))
	{
		return Existing;
	}

	// Only server can create new settings
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent::AddFloatSetting] Only server can create settings."));
		return nullptr;
	}

	// Create the runtime definition that will be replicated to clients
	FEFRuntimeSettingDefinition Def;
	Def.Tag = Tag;
	Def.DisplayName = DisplayName;
	Def.ConfigCategory = ConfigCategory;
	Def.SettingType = 1; // Float
	Def.FloatDefaultValue = DefaultValue;
	Def.FloatMin = Min;
	Def.FloatMax = Max;
	Def.FloatValue = FMath::Clamp(InitialValue, Min, Max);
	RuntimeSettingDefinitions.Add(Def);

	// Create the setting locally on server
	UEFModularSettingsFloat* NewSetting = Cast<UEFModularSettingsFloat>(CreateSettingFromRuntimeDefinition(Def));
	
	return NewSetting;
}

UEFModularSettingsMultiSelect* UEFPlayerSettingsComponent::AddMultiSelectSetting(
	FGameplayTag Tag,
	FText DisplayName,
	FName ConfigCategory,
	const TArray<FString>& Values,
	const TArray<FText>& ValueDisplayNames,
	int32 DefaultIndex,
	int32 InitialIndex)
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent::AddMultiSelectSetting] Invalid tag provided."));
		return nullptr;
	}

	// Check if setting already exists
	if (UEFModularSettingsMultiSelect* Existing = Cast<UEFModularSettingsMultiSelect>(GetSettingByTag(Tag)))
	{
		return Existing;
	}

	// Only server can create new settings
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent::AddMultiSelectSetting] Only server can create settings."));
		return nullptr;
	}

	// Create the runtime definition that will be replicated to clients
	FEFRuntimeSettingDefinition Def;
	Def.Tag = Tag;
	Def.DisplayName = DisplayName;
	Def.ConfigCategory = ConfigCategory;
	Def.SettingType = 2; // MultiSelect
	Def.MultiSelectValues = Values;
	Def.MultiSelectDisplayNames = ValueDisplayNames;
	Def.MultiSelectDefaultValue = Values.IsValidIndex(DefaultIndex) ? Values[DefaultIndex] : TEXT("");
	Def.MultiSelectIndex = Values.IsValidIndex(InitialIndex) ? InitialIndex : 0;
	RuntimeSettingDefinitions.Add(Def);

	// Create the setting locally on server
	UEFModularSettingsMultiSelect* NewSetting = Cast<UEFModularSettingsMultiSelect>(CreateSettingFromRuntimeDefinition(Def));
	
	return NewSetting;
}

UEFModularSettingsBase* UEFPlayerSettingsComponent::CreateSettingFromRuntimeDefinition(const FEFRuntimeSettingDefinition& Def)
{
	if (!Def.Tag.IsValid())
	{
		return nullptr;
	}

	// Already exists?
	if (UEFModularSettingsBase* ExistingSetting = GetSettingByTag(Def.Tag))
	{
		return ExistingSetting;
	}

	UEFModularSettingsBase* NewSetting = nullptr;

	switch (Def.SettingType)
	{
	case 0: // Bool
		{
			UEFModularSettingsBool* BoolSetting = NewObject<UEFModularSettingsBool>(this);
			BoolSetting->SettingTag = Def.Tag;
			BoolSetting->DisplayName = Def.DisplayName;
			BoolSetting->ConfigCategory = Def.ConfigCategory;
			BoolSetting->DefaultValue = Def.BoolDefaultValue;
			BoolSetting->Value = Def.BoolValue;
			NewSetting = BoolSetting;
		}
		break;

	case 1: // Float
		{
			UEFModularSettingsFloat* FloatSetting = NewObject<UEFModularSettingsFloat>(this);
			FloatSetting->SettingTag = Def.Tag;
			FloatSetting->DisplayName = Def.DisplayName;
			FloatSetting->ConfigCategory = Def.ConfigCategory;
			FloatSetting->DefaultValue = Def.FloatDefaultValue;
			FloatSetting->Min = Def.FloatMin;
			FloatSetting->Max = Def.FloatMax;
			FloatSetting->Value = Def.FloatValue;
			NewSetting = FloatSetting;
		}
		break;

	case 2: // MultiSelect
		{
			UEFModularSettingsMultiSelect* MultiSetting = NewObject<UEFModularSettingsMultiSelect>(this);
			MultiSetting->SettingTag = Def.Tag;
			MultiSetting->DisplayName = Def.DisplayName;
			MultiSetting->ConfigCategory = Def.ConfigCategory;
			MultiSetting->Values = Def.MultiSelectValues;
			MultiSetting->DisplayNames = Def.MultiSelectDisplayNames;
			MultiSetting->DefaultValue = Def.MultiSelectDefaultValue;
			MultiSetting->SelectedIndex = Def.MultiSelectIndex;
			NewSetting = MultiSetting;
		}
		break;
	}

	if (NewSetting)
	{
		Settings.Add(NewSetting);

		// Only register as replicated subobject on server
		if (GetOwnerRole() == ROLE_Authority)
		{
			if (AActor* Owner = GetOwner())
			{
				Owner->AddReplicatedSubObject(NewSetting);
			}
		}

		OnSettingChanged.Broadcast(NewSetting);
	}

	return NewSetting;
}

void UEFPlayerSettingsComponent::OnRep_RuntimeSettingDefinitions()
{
	// Build any missing settings from replicated runtime definitions.
	for (const FEFRuntimeSettingDefinition& Def : RuntimeSettingDefinitions)
	{
		if (!Def.Tag.IsValid())
		{
			continue;
		}

		if (GetSettingByTag(Def.Tag))
		{
			continue;
		}

		CreateSettingFromRuntimeDefinition(Def);
	}
}
