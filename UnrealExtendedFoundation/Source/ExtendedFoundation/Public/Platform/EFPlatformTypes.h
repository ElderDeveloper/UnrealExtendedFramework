// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "EFPlatformTypes.generated.h"

/** A service the platform layer routes to one provider (Steam, EOS, PlayFab, Local, ...). */
UENUM(BlueprintType)
enum class EEFPlatformFeature : uint8
{
	Achievements,
	Stats,
	Leaderboards,
	CloudStorage,
	Presence
};

/** How a stat update changes the stat. */
UENUM(BlueprintType)
enum class EEFStatUpdateMode : uint8
{
	/**
	 * Report the new value. Providers that aggregate on their server (EOS, PlayFab) apply the
	 * aggregation configured there (sum, max, min, latest).
	 */
	Set,
	/** Add the value to the current value. */
	Increment
};

/** Which entries a leaderboard query returns. */
UENUM(BlueprintType)
enum class EEFLeaderboardRange : uint8
{
	/** The best entries, from rank 1. */
	Top,
	/** Entries around the local user's rank, including the user. */
	AroundUser,
	/** The local user's friends, including the user. */
	Friends
};

/** Which list of the platform catalog an id belongs to. */
UENUM(BlueprintType)
enum class EEFPlatformCatalogKind : uint8
{
	Achievement,
	Stat,
	Leaderboard
};

/** The user a provider acts for. */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFPlatformUser
{
	GENERATED_BODY()

	/** The provider that reported the user: Steam, EOS, PlayFab, Local. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FName Provider;

	/** The provider's id for the user: SteamID64, EOS ProductUserId, PlayFabId, or a local id. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FString UserId;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FString DisplayName;

	/** False while the provider has no signed-in user. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	bool bSignedIn = false;
};

USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFAchievementState
{
	GENERATED_BODY()

	/** The game's id. Providers leave it empty; the platform subsystem fills it in from the catalog. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FName Id;

	/** The provider's id for the achievement. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FString ProviderId;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	bool bUnlocked = false;

	/** 0 to 1; 1 when unlocked, 0 when the provider reports no partial progress. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	float Progress = 0.f;

	/** When it was unlocked, in UTC, if the provider reports it. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FDateTime UnlockTimeUtc;

	/** The provider's display name, or the catalog's when the provider has none. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FText DisplayName;
};

USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFStatValue
{
	GENERATED_BODY()

	/** The game's id, filled in by the platform subsystem. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FName Id;

	/** The provider's id for the stat. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FString ProviderId;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	int64 Value = 0;
};

/** One change to a stat. Providers that store 32-bit stats clamp the value. */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFStatUpdate
{
	GENERATED_BODY()

	/** The game's id for the stat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform")
	FName Id;

	/** The provider's id. Filled in from the catalog before the update reaches a provider. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FString ProviderId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform")
	int64 Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform")
	EEFStatUpdateMode Mode = EEFStatUpdateMode::Set;
};

USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFLeaderboardEntry
{
	GENERATED_BODY()

	/** 1 for the best entry. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FString UserId;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	int64 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	bool bIsLocalUser = false;
};

USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFCloudFileInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	int64 SizeBytes = 0;

	/** Last change in UTC, when the provider reports it. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FDateTime ModifiedUtc;
};

namespace EFPlatform
{
	/** "Achievements", "Stats", ... */
	EXTENDEDFOUNDATION_API const TCHAR* FeatureToString(EEFPlatformFeature Feature);

	/** Every feature, for loops. */
	EXTENDEDFOUNDATION_API TConstArrayView<EEFPlatformFeature> AllFeatures();
}
