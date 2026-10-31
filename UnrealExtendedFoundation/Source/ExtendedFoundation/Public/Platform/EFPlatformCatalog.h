// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Platform/EFPlatformTypes.h"
#include "EFPlatformCatalog.generated.h"

/** One achievement, stat or leaderboard, under the id the game uses and the id each store uses. */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFPlatformCatalogEntry
{
	GENERATED_BODY()

	/** The id game code and Blueprints use, e.g. FirstBlood. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FName Id;

	/** Shown by the local provider and debug UI; stores show their own names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FText DisplayName;

	/**
	 * The id on each provider that names it differently, keyed by provider name (Steam, EOS,
	 * PlayFab, Local). A provider that is not listed uses Id as it is.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	TMap<FName, FString> ProviderIds;
};

/**
 * Maps the game's achievement, stat and leaderboard ids to each store's ids, so game code calls
 * Unlock Achievement "FirstBlood" once and every store receives its own name for it.
 *
 * Assign it in Project Settings > Extended Framework > Extended Platform. Ids that are not in the
 * catalog are sent to providers unchanged.
 */
UCLASS(BlueprintType)
class EXTENDEDFOUNDATION_API UEFPlatformCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog", meta = (TitleProperty = "Id"))
	TArray<FEFPlatformCatalogEntry> Achievements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog", meta = (TitleProperty = "Id"))
	TArray<FEFPlatformCatalogEntry> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog", meta = (TitleProperty = "Id"))
	TArray<FEFPlatformCatalogEntry> Leaderboards;

	const TArray<FEFPlatformCatalogEntry>& GetEntries(EEFPlatformCatalogKind Kind) const;

	const FEFPlatformCatalogEntry* FindEntry(EEFPlatformCatalogKind Kind, FName Id) const;

	/** Provider's id for Id, or Id as a string when the catalog has no entry or no override for Provider. */
	FString ToProviderId(EEFPlatformCatalogKind Kind, FName Id, FName Provider) const;

	/** The game id whose id on Provider is ProviderId, or ProviderId as a name when none matches. */
	FName FromProviderId(EEFPlatformCatalogKind Kind, const FString& ProviderId, FName Provider) const;

	/** Entries that share an Id, or give two ids the same provider id. Empty when the catalog is consistent. */
	TArray<FString> FindProblems() const;
};
