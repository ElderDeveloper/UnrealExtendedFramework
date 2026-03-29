// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSSanctionsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSanctionsQueried, bool, bSuccess, const TArray<FEEOSSanction>&, Sanctions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSPlayerReportSent, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSSanctionAppealSent, bool, bSuccess);

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

	/** Query active sanctions for a user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sanctions")
	void QueryActiveSanctions(const FString& TargetUserId);

	/** Send a player behavior report */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sanctions")
	void SendPlayerReport(const FString& TargetUserId, const FString& Reason, const FString& Message);

	/** Send a player report with a specific category */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sanctions")
	void SendPlayerReportWithCategory(const FString& TargetUserId, const FString& Category, const FString& Reason, const FString& Message);

	/** Submit an appeal for a sanction */
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

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sanctions")
	FOnEOSSanctionsQueried OnSanctionsQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sanctions")
	FOnEOSPlayerReportSent OnPlayerReportSent;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sanctions")
	FOnEOSSanctionAppealSent OnSanctionAppealSent;

private:

	TArray<FEEOSSanction> CachedSanctions;
};
