// Fill out your copyright notice in the Description page of Project Settings.


#include "EFModularSettingsLibrary.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "UnrealExtendedFramework/ModularSettings/Components/EFPlayerSettingsComponent.h"
#include "UnrealExtendedFramework/ModularSettings/Components/EFWorldSettingsComponent.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

UEFModularSettingsBase* UEFModularSettingsLibrary::GetModularSetting(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return nullptr;

	auto TryPlayer = [&]() -> UEFModularSettingsBase*
	{
		APlayerState* TargetPlayer = SpecificPlayer;
		if (!TargetPlayer && World->GetFirstPlayerController())
		{
			TargetPlayer = World->GetFirstPlayerController()->PlayerState;
		}

		if (TargetPlayer)
		{
			if (UEFPlayerSettingsComponent* PlayerComp = TargetPlayer->FindComponentByClass<UEFPlayerSettingsComponent>())
			{
				return PlayerComp->GetSettingByTag(Tag);
			}
		}
		return nullptr;
	};

	auto TryWorld = [&]() -> UEFModularSettingsBase*
	{
		if (AGameStateBase* GS = World->GetGameState())
		{
			if (UEFWorldSettingsComponent* WorldComp = GS->FindComponentByClass<UEFWorldSettingsComponent>())
			{
				return WorldComp->GetSettingByTag(Tag);
			}
		}
		return nullptr;
	};

	auto TryLocal = [&]() -> UEFModularSettingsBase*
	{
		if (World->GetGameInstance())
		{
			if (UEFModularSettingsSubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
			{
				return Subsystem->GetSettingByTag(Tag);
			}
		}
		return nullptr;
	};

	switch (Source)
	{
	case EEFSettingsSource::Local:
		return TryLocal();
	case EEFSettingsSource::World:
		return TryWorld();
	case EEFSettingsSource::Player:
		return TryPlayer();
	case EEFSettingsSource::Auto:
	default:
		if (UEFModularSettingsBase* Setting = TryPlayer()) return Setting;
		if (UEFModularSettingsBase* Setting = TryWorld()) return Setting;
		return TryLocal();
	}
}

bool UEFModularSettingsLibrary::GetModularBool(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	if (UEFModularSettingsBool* Setting = Cast<UEFModularSettingsBool>(GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer)))
	{
		return Setting->Value;
	}
	return false;
}

void UEFModularSettingsLibrary::SetModularBool(const UObject* WorldContextObject, FGameplayTag Tag, bool bValue, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	UEFModularSettingsBase* SettingBase = GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer);
	if (UEFModularSettingsBool* Setting = Cast<UEFModularSettingsBool>(SettingBase))
	{
		FString ValStr = bValue ? TEXT("true") : TEXT("false");
		if (Setting->GetOuter()->IsA<UEFPlayerSettingsComponent>())
		{
			Cast<UEFPlayerSettingsComponent>(Setting->GetOuter())->RequestUpdateSetting(Tag, ValStr);
		}
		else if (Setting->GetOuter()->IsA<UEFWorldSettingsComponent>())
		{
			Cast<UEFWorldSettingsComponent>(Setting->GetOuter())->UpdateSettingValue(Tag, ValStr);
		}
		else
		{
			Setting->SetValue(bValue);
			Setting->Apply();
			
			UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
			if (World && World->GetGameInstance())
			{
				if (UEFModularSettingsSubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
				{
					Subsystem->OnSettingsChanged.Broadcast(Setting);
				}
			}
		}
	}
}

float UEFModularSettingsLibrary::GetModularFloat(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	if (UEFModularSettingsFloat* Setting = Cast<UEFModularSettingsFloat>(GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer)))
	{
		return Setting->Value;
	}
	return 0.0f;
}

void UEFModularSettingsLibrary::SetModularFloat(const UObject* WorldContextObject, FGameplayTag Tag, float Value, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	UEFModularSettingsBase* SettingBase = GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer);
	if (UEFModularSettingsFloat* Setting = Cast<UEFModularSettingsFloat>(SettingBase))
	{
		FString ValStr = FString::SanitizeFloat(Value);
		if (Setting->GetOuter()->IsA<UEFPlayerSettingsComponent>())
		{
			Cast<UEFPlayerSettingsComponent>(Setting->GetOuter())->RequestUpdateSetting(Tag, ValStr);
		}
		else if (Setting->GetOuter()->IsA<UEFWorldSettingsComponent>())
		{
			Cast<UEFWorldSettingsComponent>(Setting->GetOuter())->UpdateSettingValue(Tag, ValStr);
		}
		else
		{
			Setting->SetValue(Value);
			Setting->Apply();

			UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
			if (World && World->GetGameInstance())
			{
				if (UEFModularSettingsSubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
				{
					Subsystem->OnSettingsChanged.Broadcast(Setting);
				}
			}
		}
	}
}

int32 UEFModularSettingsLibrary::GetModularSelectedIndex(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	if (UEFModularSettingsMultiSelect* Setting = Cast<UEFModularSettingsMultiSelect>(GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer)))
	{
		return Setting->SelectedIndex;
	}
	return INDEX_NONE;
}

bool UEFModularSettingsLibrary::SetModularSelectedIndex(const UObject* WorldContextObject, FGameplayTag Tag, int32 Index, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	UEFModularSettingsBase* SettingBase = GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer);
	if (UEFModularSettingsMultiSelect* Setting = Cast<UEFModularSettingsMultiSelect>(SettingBase))
	{
		if (Setting->Values.IsValidIndex(Index))
		{
			FString OptionValue = Setting->Values[Index];
			if (Setting->GetOuter()->IsA<UEFPlayerSettingsComponent>())
			{
				Cast<UEFPlayerSettingsComponent>(Setting->GetOuter())->RequestUpdateSetting(Tag, OptionValue);
			}
			else if (Setting->GetOuter()->IsA<UEFWorldSettingsComponent>())
			{
				Cast<UEFWorldSettingsComponent>(Setting->GetOuter())->UpdateSettingValue(Tag, OptionValue);
			}
			else
			{
				Setting->SetSelectedIndex(Index);
				Setting->Apply();

				UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
				if (World && World->GetGameInstance())
				{
					if (UEFModularSettingsSubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
					{
						Subsystem->OnSettingsChanged.Broadcast(Setting);
					}
				}
			}
			return true;
		}
	}
	return false;
}

FString UEFModularSettingsLibrary::GetModularSelectedOption(const UObject* WorldContextObject, FGameplayTag Tag,EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	if (UEFModularSettingsMultiSelect* Setting = Cast<UEFModularSettingsMultiSelect>(GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer)))
	{
		return Setting->GetValueAsString();
	}
	return TEXT("");
}

void UEFModularSettingsLibrary::AdjustModularIndex(const UObject* WorldContextObject, FGameplayTag Tag, int32 Amount, bool bWrap, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	UEFModularSettingsBase* SettingBase = GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer);
	if (UEFModularSettingsMultiSelect* MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(SettingBase))
	{
		int32 NumOptions = MultiSelectSetting->Values.Num();
		if (NumOptions <= 0) return;

		int32 CurrentIndex = MultiSelectSetting->SelectedIndex;
		int32 NewIndex = CurrentIndex;
		
		int32 Step = Amount > 0 ? 1 : -1;
		int32 AbsAmount = FMath::Abs(Amount);

		for (int32 i = 0; i < AbsAmount; ++i)
		{
			// Try to find the next available index
			int32 NextCandidate = NewIndex;
			bool bFound = false;

			// We try up to NumOptions times to find an unlocked one
			for (int32 Attempt = 0; Attempt < NumOptions; ++Attempt)
			{
				NextCandidate += Step;

				if (bWrap)
				{
					if (NextCandidate < 0) NextCandidate = NumOptions - 1;
					else if (NextCandidate >= NumOptions) NextCandidate = 0;
				}
				else
				{
					if (NextCandidate < 0 || NextCandidate >= NumOptions)
					{
						// Can't move further
						break;
					}
				}

				if (!MultiSelectSetting->IsIndexLocked(NextCandidate))
				{
					NewIndex = NextCandidate;
					bFound = true;
					break;
				}
			}

			if (!bFound) break;
		}

		if (NewIndex != CurrentIndex)
		{
			SetModularSelectedIndex(WorldContextObject, Tag, NewIndex, Source, SpecificPlayer);
		}
	}
}


TArray<FText> UEFModularSettingsLibrary::GetModularOptions(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	if (UEFModularSettingsMultiSelect* Setting = Cast<UEFModularSettingsMultiSelect>(GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer)))
	{
		return Setting->DisplayNames;
	}
	return TArray<FText>();
}

void UEFModularSettingsLibrary::ApplyModularSetting(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	if (UEFModularSettingsBase* Setting = GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer))
	{
		Setting->Apply();
	}
}

bool UEFModularSettingsLibrary::IsModularOptionLocked(const UObject* WorldContextObject, FGameplayTag Tag, int32 Index, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	if (UEFModularSettingsMultiSelect* Setting = Cast<UEFModularSettingsMultiSelect>(GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer)))
	{
		return Setting->IsIndexLocked(Index);
	}
	return false;
}

void UEFModularSettingsLibrary::SetModularOptionLocked(const UObject* WorldContextObject, FGameplayTag Tag, int32 Index, bool bLocked, EEFSettingsSource Source, APlayerState* SpecificPlayer)
{
	if (UEFModularSettingsMultiSelect* Setting = Cast<UEFModularSettingsMultiSelect>(GetModularSetting(WorldContextObject, Tag, Source, SpecificPlayer)))
	{
		if (Setting->Values.IsValidIndex(Index))
		{
			Setting->SetOptionLocked(Setting->Values[Index], bLocked);
		}
	}
}
