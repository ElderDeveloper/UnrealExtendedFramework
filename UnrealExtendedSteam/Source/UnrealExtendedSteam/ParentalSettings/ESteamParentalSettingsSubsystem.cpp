// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ParentalSettings/ESteamParentalSettingsSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	EParentalFeature ToSteamParentalFeature(EESteamParentalFeature Feature)
	{
		switch (Feature)
		{
		case EESteamParentalFeature::Store:         return k_EFeatureStore;
		case EESteamParentalFeature::Community:     return k_EFeatureCommunity;
		case EESteamParentalFeature::Profile:       return k_EFeatureProfile;
		case EESteamParentalFeature::Friends:       return k_EFeatureFriends;
		case EESteamParentalFeature::News:          return k_EFeatureNews;
		case EESteamParentalFeature::Trading:       return k_EFeatureTrading;
		case EESteamParentalFeature::Settings:      return k_EFeatureSettings;
		case EESteamParentalFeature::Console:       return k_EFeatureConsole;
		case EESteamParentalFeature::Browser:       return k_EFeatureBrowser;
		case EESteamParentalFeature::ParentalSetup: return k_EFeatureParentalSetup;
		case EESteamParentalFeature::Library:       return k_EFeatureLibrary;
		case EESteamParentalFeature::Test:          return k_EFeatureTest;
		case EESteamParentalFeature::SiteLicense:   return k_EFeatureSiteLicense;
#if ESTEAM_SDK_AT_LEAST(164)
		case EESteamParentalFeature::KioskMode:     return k_EFeatureKioskMode_Deprecated;
		case EESteamParentalFeature::BlockAlways:   return k_EFeatureBlockAlways;
		case EESteamParentalFeature::Desktop:       return k_EFeatureDesktop;
#endif
		default:                                    return k_EFeatureInvalid;
		}
	}
}

/** Native Steam callback listener; alive only while the Steam client API is initialized. */
class FESteamParentalSettingsCallbacks
{
public:
	explicit FESteamParentalSettingsCallbacks(UESteamParentalSettingsSubsystem* InOwner)
		: Owner(InOwner)
		, SettingsChanged(this, &FESteamParentalSettingsCallbacks::HandleSettingsChanged)
	{
	}

private:
	void HandleSettingsChanged(SteamParentalSettingsChanged_t* Data)
	{
		if (UESteamParentalSettingsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnParentalSettingsChanged.Broadcast();
		}
	}

	TWeakObjectPtr<UESteamParentalSettingsSubsystem> Owner;
	CCallback<FESteamParentalSettingsCallbacks, SteamParentalSettingsChanged_t> SettingsChanged;
};
#else
class FESteamParentalSettingsCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamParentalSettingsSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamParentalSettingsSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamParentalSettingsCallbacks>(this);
	}
#endif
}

void UESteamParentalSettingsSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

bool UESteamParentalSettingsSubsystem::IsParentalLockEnabled() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamParentalSettings() && SteamParentalSettings()->BIsParentalLockEnabled();
#else
	return false;
#endif
}

bool UESteamParentalSettingsSubsystem::IsParentalLockLocked() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamParentalSettings() && SteamParentalSettings()->BIsParentalLockLocked();
#else
	return false;
#endif
}

bool UESteamParentalSettingsSubsystem::IsAppBlocked(int32 AppId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamParentalSettings()
		&& SteamParentalSettings()->BIsAppBlocked(static_cast<AppId_t>(AppId));
#else
	return false;
#endif
}

bool UESteamParentalSettingsSubsystem::IsAppInBlockList(int32 AppId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamParentalSettings()
		&& SteamParentalSettings()->BIsAppInBlockList(static_cast<AppId_t>(AppId));
#else
	return false;
#endif
}

bool UESteamParentalSettingsSubsystem::IsFeatureBlocked(EESteamParentalFeature Feature) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamParentalSettings()
		&& SteamParentalSettings()->BIsFeatureBlocked(ToSteamParentalFeature(Feature));
#else
	return false;
#endif
}

bool UESteamParentalSettingsSubsystem::IsFeatureInBlockList(EESteamParentalFeature Feature) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamParentalSettings()
		&& SteamParentalSettings()->BIsFeatureInBlockList(ToSteamParentalFeature(Feature));
#else
	return false;
#endif
}
