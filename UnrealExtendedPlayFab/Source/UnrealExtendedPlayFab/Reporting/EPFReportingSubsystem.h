// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFReportingSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFPlayerReported, const FEPFResult&, Result);

/**
 * Player Reporting — report toxic or abusive players to PlayFab.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFReportingSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Report a player for abusive behavior */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Reporting")
	void ReportPlayer(const FString& ReporteePlayFabId, const FString& Comment = TEXT(""));

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Reporting")
	FOnEPFPlayerReported OnPlayerReported;
};
