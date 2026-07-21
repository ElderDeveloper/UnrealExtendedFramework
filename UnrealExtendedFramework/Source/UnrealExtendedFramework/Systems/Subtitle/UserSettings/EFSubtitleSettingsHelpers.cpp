// EFSubtitleSettingsHelpers.cpp
#include "EFSubtitleSettingsHelpers.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UnrealExtendedFramework/ModularSettings/Components/EFPlayerSettingsComponent.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleProjectSettings.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleStyleProfile.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Subsystem/EFSubtitleLocalSubsystem.h"

namespace EFSubtitleSettingsHelpers
{
	UEFSubtitleLocalSubsystem* ResolveLocalSubsystem(const UEFModularSettingsBase* Setting)
	{
		if (!Setting)
		{
			return nullptr;
		}

		const UObject* Outer = Setting->GetOuter();
		while (Outer)
		{
			if (const APlayerController* PC = Cast<APlayerController>(Outer))
			{
				if (ULocalPlayer* LP = PC->GetLocalPlayer())
				{
					return LP->GetSubsystem<UEFSubtitleLocalSubsystem>();
				}
			}

			if (const UEFPlayerSettingsComponent* PlayerComp = Cast<UEFPlayerSettingsComponent>(Outer))
			{
				if (AActor* Owner = PlayerComp->GetOwner())
				{
					APlayerController* PC = Cast<APlayerController>(Owner);
					if (!PC)
					{
						if (APawn* Pawn = Cast<APawn>(Owner))
						{
							PC = Cast<APlayerController>(Pawn->GetController());
						}
					}

					if (PC)
					{
						if (ULocalPlayer* LP = PC->GetLocalPlayer())
						{
							return LP->GetSubsystem<UEFSubtitleLocalSubsystem>();
						}
					}
				}
			}

			Outer = Outer->GetOuter();
		}

		if (UWorld* World = Setting->GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (ULocalPlayer* LP = GI->GetFirstGamePlayer())
				{
					return LP->GetSubsystem<UEFSubtitleLocalSubsystem>();
				}
			}
		}

		return nullptr;
	}

	UEFSubtitleStyleProfile* ResolveStyleProfile(FName ProfileId)
	{
		if (ProfileId.IsNone())
		{
			return nullptr;
		}

		const UEFSubtitleProjectSettings* Settings = GetDefault<UEFSubtitleProjectSettings>();
		if (!Settings)
		{
			return nullptr;
		}

		for (const TSoftObjectPtr<UEFSubtitleStyleProfile>& SoftProfile : Settings->AvailableStyleProfiles)
		{
			if (UEFSubtitleStyleProfile* Profile = SoftProfile.LoadSynchronous())
			{
				if (Profile->ProfileId == ProfileId)
				{
					return Profile;
				}
			}
		}

		return nullptr;
	}
}
