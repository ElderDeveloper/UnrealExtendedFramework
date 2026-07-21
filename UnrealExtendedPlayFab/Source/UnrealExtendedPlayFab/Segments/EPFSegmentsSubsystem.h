// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFSegmentsSubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFPlayerSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Segments")
	FString SegmentId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Segments")
	FString SegmentName;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFSegmentsReceived, const FEPFResult&, Result, const TArray<FEPFPlayerSegment>&, Segments);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFPlayerTagsReceived, const FEPFResult&, Result, const TArray<FString>&, Tags);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFPlayerTagModified, const FEPFResult&, Result);

/**
 * Player Segments — query which segments the player belongs to.
 * Segments are configured in the PlayFab dashboard. This subsystem queries
 * membership and player tags for conditional content, offers, and A/B testing.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFSegmentsSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Fetch all segments the current player belongs to */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Segments")
	void GetPlayerSegments();

	/** Fetch all tags for the current player */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Segments")
	void GetPlayerTags(const FString& Namespace = TEXT(""));

	/** Add a tag to the current player (useful for custom segmentation) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Segments")
	void AddPlayerTag(const FString& TagName, const FString& Namespace = TEXT(""));

	/** Remove a tag from the current player */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Segments")
	void RemovePlayerTag(const FString& TagName, const FString& Namespace = TEXT(""));

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if the player is in a specific segment (cached) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Segments")
	bool IsInSegment(const FString& SegmentName) const;

	/** Check if the player is in a segment by ID (cached) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Segments")
	bool IsInSegmentById(const FString& SegmentId) const;

	/** Get all cached segments */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Segments")
	TArray<FEPFPlayerSegment> GetCachedSegments() const;

	/** Check if the player has a specific tag (cached) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Segments")
	bool HasTag(const FString& TagName) const;

	/** Get all cached tags */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Segments")
	TArray<FString> GetCachedTags() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Segments")
	FOnEPFSegmentsReceived OnSegmentsReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Segments")
	FOnEPFPlayerTagsReceived OnPlayerTagsReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Segments")
	FOnEPFPlayerTagModified OnPlayerTagModified;

private:

	TArray<FEPFPlayerSegment> CachedSegments;
	TArray<FString> CachedTags;
};
