// Copyright Moon Punch Games. All Rights Reserved.

#include "Settings/EFDeveloperSettings.h"

#include "ExtendedFoundationModule.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#endif

void UEFDeveloperSettings::OpenInEditor() const
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
	{
		SettingsModule->ShowViewer(GetContainerName(), GetCategoryName(), GetSectionName());
	}
#endif
}

TArray<FString> UEFDeveloperSettings::FindShippedSecrets() const
{
	TArray<FString> Shipped;

#if WITH_EDITOR
	// Only Default*.ini files are staged into builds; per-user Saved/Config files are not.
	if (!GetClass()->HasAnyClassFlags(CLASS_DefaultConfig))
	{
		return Shipped;
	}

	static const FName SecretMeta(TEXT("EFSecret"));
	for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasMetaData(SecretMeta) || !Property->HasAnyPropertyFlags(CPF_Config) || Property->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		bool bHasValue = false;
		if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			bHasValue = !StringProperty->GetPropertyValue_InContainer(this).IsEmpty();
		}
		else if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			bHasValue = !TextProperty->GetPropertyValue_InContainer(this).IsEmpty();
		}
		else if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			bHasValue = !NameProperty->GetPropertyValue_InContainer(this).IsNone();
		}
		else
		{
			// Any other type: compare against the class default to see whether someone set it.
			bHasValue = !Property->Identical_InContainer(this, GetClass()->GetDefaultObject(false));
		}

		if (bHasValue)
		{
			Shipped.Add(Property->GetName());
		}
	}
#endif

	return Shipped;
}

void UEFDeveloperSettings::WarnAboutShippedSecrets() const
{
	for (const FString& PropertyName : FindShippedSecrets())
	{
		UE_LOG(LogExtendedFoundation, Warning,
			TEXT("%s.%s is marked as a secret but is saved to Default%s.ini, which ships with the game. Keep it in FEFCredentialStore or on a server instead."),
			*GetClass()->GetName(), *PropertyName, *GetClass()->ClassConfigName.ToString());
	}
}

#if WITH_EDITOR
void UEFDeveloperSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// The class default object carries the loaded config; instances are copies of it.
	if (HasAnyFlags(RF_ClassDefaultObject) && !GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		WarnAboutShippedSecrets();
	}
}

void UEFDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FName SecretMeta(TEXT("EFSecret"));
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->HasMetaData(SecretMeta))
	{
		WarnAboutShippedSecrets();
	}
}
#endif
