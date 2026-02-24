// EFSubtitleDataAsset.h — DataAsset alternative to DataTable for subtitle data
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EFSubtitleData.h"
#include "EFSubtitleDataAsset.generated.h"

/**
 * A DataAsset containing subtitle entries.
 * Provides a modular alternative to a single monolithic DataTable.
 * Game Feature Plugins can each ship their own DataAsset.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// All subtitle entries in this asset, keyed by subtitle key (FName)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle Data")
	TMap<FName, FEFSubtitleEntry> Entries;

	// Category tag for this asset (e.g., Dialogue.Shop, Dialogue.Quest)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle Data")
	FGameplayTag Category;

	// Look up an entry by key. Returns nullptr if not found.
	const FEFSubtitleEntry* FindEntry(FName Key) const
	{
		return Entries.Find(Key);
	}

	// Get the primary asset ID for asset manager registration
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("SubtitleDataAsset", GetFName());
	}
};
