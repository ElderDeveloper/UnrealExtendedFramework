// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSSanctionsSubsystem.generated.h"

// Target-scoped delegates carry the id they were invoked for so concurrent
// operations against different targets can be correlated by listeners.
// TargetUserId is always the BARE Product User ID (composite "<EAS>|<PUID>"
// inputs are normalized), or an empty string when the input had no parseable PUID.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSSanctionsQueried, bool, bSuccess, const FString&, TargetUserId, const TArray<FEEOSSanction>&, Sanctions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerReportSent, bool, bSuccess, const FString&, TargetUserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSanctionAppealSent, bool, bSuccess, const FString&, SanctionId);

/**
 * Manages EOS player reports, sanctions queries, and the sanctions appeal system.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSSanctionsSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/**
	 * Query active sanctions for a user. Result arrives via OnSanctionsQueried
	 * with the queried target's bare PUID for correlation.
	 * FAIL-CLOSED cache semantics: CachedSanctions is rebuilt ONLY on a successful
	 * query — a failed re-query keeps the last-known-good list, so a previously
	 * sanctioned player never reads as clean because of a transient failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sanctions")
	void QueryActiveSanctions(const FString& TargetUserId);

	/** Send a player behavior report */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sanctions")
	void SendPlayerReport(const FString& TargetUserId, const FString& Reason, const FString& Message);

	/** Send a player report with a specific category */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sanctions")
	void SendPlayerReportWithCategory(const FString& TargetUserId, const FString& Category, const FString& Reason, const FString& Message);

	/**
	 * Submit an appeal for a sanction. Result arrives via OnSanctionAppealSent
	 * with the SanctionId for correlation.
	 * NOTE: AppealMessage is NOT transmitted — the EOS SDK appeal API
	 * (EOS_Sanctions_CreatePlayerSanctionAppeal) has no message field. The
	 * parameter is kept for future SDK support and is logged locally only.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sanctions")
	void AppealSanction(const FString& SanctionId, EEOSSanctionAppealType AppealType, const FString& AppealMessage);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached sanctions from the last query */
	UFUNCTION(BlueprintPure, Category = "EOS|Sanctions")
	TArray<FEEOSSanction> GetCachedSanctions() const;

	/** Check if a user has any active sanctions */
	UFUNCTION(BlueprintPure, Category = "EOS|Sanctions")
	bool HasActiveSanctions() const;

	/** Get a specific sanction by ID from cache */
	UFUNCTION(BlueprintPure, Category = "EOS|Sanctions")
	bool GetSanctionById(const FString& SanctionId, FEEOSSanction& OutSanction) const;

	/** Get the count of active sanctions */
	UFUNCTION(BlueprintPure, Category = "EOS|Sanctions")
	int32 GetSanctionCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	/**
	 * Query result. TargetUserId = bare PUID of the queried user (empty when the
	 * input had no parseable PUID). On failure (bSuccess=false) Sanctions is the
	 * RETAINED last-known-good cached list, not a fresh result — fail closed.
	 */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Sanctions")
	FOnEOSSanctionsQueried OnSanctionsQueried;

	/** Report result. TargetUserId = bare PUID of the reported user (empty when the input had no parseable PUID). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Sanctions")
	FOnEOSPlayerReportSent OnPlayerReportSent;

	/** Appeal result. SanctionId = the sanction ReferenceId the appeal was submitted for. */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Sanctions")
	FOnEOSSanctionAppealSent OnSanctionAppealSent;

private:

	/**
	 * Sanctions from the most recent SUCCESSFUL query (single-slot, last target
	 * wins). Never cleared by a failed re-query — HasActiveSanctions() and friends
	 * fail CLOSED on transient backend errors.
	 */
	TArray<FEEOSSanction> CachedSanctions;
};
